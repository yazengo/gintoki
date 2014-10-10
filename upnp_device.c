
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <upnp.h>
#include <lua.h>
#include <lauxlib.h>

#include "upnp_device.h"
#include "upnp_util.h"
#include "utils.h"

#define STR_DEVICE_DESC_XML "munodevicedesc.xml"
#define PLAYER_SERVICE_TYPE "urn:schemas-upnp-org:service:munocontrol:1"
#define DEFAULT_WEB_DIR "/usr/app/web"
#define DESC_URL_SIZE 200
#define DEFAULT_ADVR_EXPIRE 100

typedef struct {
	lua_State *L;
	uv_loop_t *loop;
	pthread_mutex_t lock;
	UpnpDevice_Handle h;
	char *srv, *sid, *udn;
} upnp_t;

static upnp_t _upnp = {
	.lock = PTHREAD_MUTEX_INITIALIZER
}, *upnp = &_upnp;

typedef struct {
	char *srv, *sid, *udn;
	char *in;
	char *out;
	const char *method;
} upnp_luv_t;

// arg[1] = [userptr ul]
// arg[2] = ret
static int upnp_luv_action_done(lua_State *L) {
	upnp_luv_t *ul = (upnp_luv_t *)lua_touserptr(L, 1);

	ul->out = (char *)lua_tostring(L, 2);
	if (ul->out)
		ul->out = strdup(ul->out);

	return 0;
}

// arg[1] = [userptr ul]
// arg[2] = done
static int upnp_luv_action_start(lua_State *L) {
	upnp_luv_t *ul = (upnp_luv_t *)lua_touserptr(L, 1);

	// <method>(in, done)
	
	lua_getglobal(L, ul->method);
	if (lua_isnil(L, -1))
		return 0;

	lua_pushstring(L, ul->in);
	lua_pushvalue(L, 2);
	lua_call_or_die(L, 2, 0);

	return 0;
}

static int upnp_control_action_request(Upnp_EventType EventType, void *Event, void *Cookie) {
	struct Upnp_Action_Request *ca_event = (struct Upnp_Action_Request *)Event;

	ca_event->ErrCode = 0;
	ca_event->ActionResult = NULL;
	// ca_event->ActionName;

	char *in = SampleUtil_GetFirstDocumentItem(ca_event->ActionRequest, "Params");
	debug("in=%s", in);
	if (in == NULL)
		return -1;

	upnp_luv_t ul = {
		.udn = ca_event->DevUDN, .srv = ca_event->ServiceID,
		.in = in, .method = "upnp_on_action",
	};
	pthread_call_luv_sync_v2(
		upnp->L, upnp->loop,
		upnp_luv_action_start, upnp_luv_action_done, &ul
	);

	debug("out=%s", ul.out);
	if (ul.out == NULL)
		return -1;

	UpnpAddToActionResponse(&ca_event->ActionResult, ca_event->ActionName,
			PLAYER_SERVICE_TYPE,
			"Result", ul.out);
	free(ul.out);

	ca_event->ErrCode = UPNP_E_SUCCESS;

	return 0;
}

static int upnp_subscription_request(struct Upnp_Subscription_Request *sr_event) {
	unsigned int i = 0;
	int cmp1 = 0; 
	int cmp2 = 0; 
	int ret = 0;

	// sid=urn:upnp-org:serviceId:munocontrol1 sid=uuid:3d298f70-1dd2-11b2-bba7-d2ef32984658 udn=uuid:Upnp-munoEmulator-1_0-1234567890007

	upnp_luv_t ul = {
		.srv = sr_event->ServiceId,
		.udn = sr_event->UDN,
		.sid = sr_event->Sid,
		.in = "", .method = "upnp_on_subscribe",
	};
	debug("srv=%s sid=%s udn=%s", ul.srv, ul.sid, ul.udn);
	pthread_call_luv_sync_v2(upnp->L, upnp->loop, upnp_luv_action_start, upnp_luv_action_done, &ul);
	debug("out=%s", ul.out);

	const char *names[] = { "sync_data" };
	const char *vals[] = { ul.out };

	if (ul.out == NULL)
		return 1;

	ret = UpnpAcceptSubscription(upnp->h, ul.udn, ul.srv, names, vals, 1, ul.sid);
	free(ul.out);

	if (ret == UPNP_E_SUCCESS) {
		info("ok");
	} else {
		info("fail");
	}

	return 1;
}

static int PlayerDeviceCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie) {
	switch (EventType) {
		case UPNP_EVENT_SUBSCRIPTION_REQUEST:
			info("UPNP_EVENT_SUBSCRIPTION_REQUEST");
			upnp_subscription_request((struct Upnp_Subscription_Request *)Event);
			break;
		case UPNP_CONTROL_GET_VAR_REQUEST:
			info("UPNP_CONTROL_GET_VAR_REQUEST");

			break;
		case UPNP_CONTROL_ACTION_REQUEST:
			info("UPNP_CONTROL_ACTION_REQUEST");
			upnp_control_action_request(EventType, Event, Cookie);
			break;
			/* ignore these cases, since this is not a control point */
		case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		case UPNP_DISCOVERY_SEARCH_RESULT:
		case UPNP_DISCOVERY_SEARCH_TIMEOUT:
		case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE:
		case UPNP_CONTROL_ACTION_COMPLETE:
		case UPNP_CONTROL_GET_VAR_COMPLETE:
		case UPNP_EVENT_RECEIVED:
		case UPNP_EVENT_RENEWAL_COMPLETE:
		case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
			break;
		default:
			info("unknown event:%d", EventType);
	}
	/* Print a summary of the event received */
	//SampleUtil_PrintEvent(EventType, Event);

	return 0;
}

static int PlayerDeviceStateInit(char *DescDocURL) {
	IXML_Document *DescDoc = NULL;
	int ret = UPNP_E_SUCCESS;
	char *evnturl_ctrl = NULL;
	char *ctrlurl_ctrl = NULL;
	char *srv = NULL;
	char *udn = NULL;

	/* Download description document */
	if (UpnpDownloadXmlDoc(DescDocURL, &DescDoc) != UPNP_E_SUCCESS) {
		warn("UpnpDownloadXmlDoc('%s') failed", DescDocURL);
		ret = UPNP_E_INVALID_DESC;
		goto error_handler;
	}

	/* Find the Player Control Service identifiers */
	udn = SampleUtil_GetFirstDocumentItem(DescDoc, "UDN");
	if (!SampleUtil_FindAndParseService(DescDoc, DescDocURL,
				PLAYER_SERVICE_TYPE,                                                                                           
				&srv, &evnturl_ctrl,
				&ctrlurl_ctrl)) {
		warn("FindAndParseService('%s') failed", PLAYER_SERVICE_TYPE);
		ret = UPNP_E_INVALID_DESC;
		goto error_handler;
	}

	/* set control service table */
	if (udn == NULL || srv == NULL) {
		warn("FindAndParseService failed");
		goto error_handler;
	}

	upnp->udn = strdup(udn);
	upnp->srv = strdup(srv);

error_handler:
	if (udn)
		free(udn);
	if (srv)
		free(srv);
	if (evnturl_ctrl)
		free(evnturl_ctrl);
	if (ctrlurl_ctrl)
		free(ctrlurl_ctrl);
	if (DescDoc)
		ixmlDocument_free(DescDoc);

	return ret;
}

static int PlayerDeviceStart(
		char *ip, unsigned short port,
		const char *desc_doc_name, const char *web_dir_path,
		int combo) 
{
	int ret = UPNP_E_SUCCESS;
	char desc_doc_url[DESC_URL_SIZE];

	info("init start ip=%s port=%u", ip, port);

	ret = UpnpInit(ip, port);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpInit failed: %d", ret);
		UpnpFinish();

		return ret;
	}

	ip = UpnpGetServerIpAddress();
	port = UpnpGetServerPort();

	if (!desc_doc_name) {
		if (combo) {
		} else {
			desc_doc_name = STR_DEVICE_DESC_XML;
		}
	}

	web_dir_path = getenv("UPNP_WEBROOT");
	if (web_dir_path == NULL) 
		web_dir_path = "upnpweb";

	snprintf(desc_doc_url, DESC_URL_SIZE, "http://%s:%d/%s", ip, port, desc_doc_name);

	ret = UpnpSetWebServerRootDir(web_dir_path);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpSetWebServerRootDir('%s') failed: %d", web_dir_path, ret);
		UpnpFinish();
		return ret;
	}

	ret = UpnpRegisterRootDevice(desc_doc_url, PlayerDeviceCallbackEventHandler, &upnp->h, &upnp->h);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpRegisterRootDevice failed: %d", ret);
		UpnpFinish();
		return ret;
	}
	
	PlayerDeviceStateInit(desc_doc_url);

	ret = UpnpSendAdvertisement(upnp->h, DEFAULT_ADVR_EXPIRE);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpSendAdvertisement failed: %d", ret);
		UpnpFinish();
		return ret;
	}

	info("upnp init ok");
	return UPNP_E_SUCCESS;
}

static int PlayerDeviceStop() {
	UpnpUnRegisterRootDevice(upnp->h);
	UpnpFinish();
	SampleUtil_Finish();   

	return UPNP_E_SUCCESS;
}

static int upnp_port = 49152;

static void *upnp_thread_stop(void *_) {
	PlayerDeviceStop();
	return NULL;
}

static void *upnp_thread_start(void *_) {
	PlayerDeviceStart(NULL, upnp_port, NULL, NULL, 0);
	return NULL;
}

static void upnp_notify_done(uv_work_t *req, int _) {
}

static void upnp_notify_thread(uv_work_t *w) {
	upnp_luv_t *ul = (upnp_luv_t *)w->data;

	const char *names[] = { "event_data" };
	const char *vals[] = { ul->in };

	int r = UpnpNotify(upnp->h, upnp->udn, upnp->srv, names, vals, 1);
	if (r == UPNP_E_SUCCESS) {
		info("ok");
	} else {
		info("fail");
	}

	free(ul->in);
}

// upnp_notify(str)
static int upnp_notify(lua_State *L) {
	if (upnp->srv == NULL || upnp->udn == NULL) {
		warn("failed: upnp srv or udn == NULL");
		return 0;
	}

	char *json = (char *)lua_tostring(L, 1);

	//info("udn=%s srv=%s json=%s", upnp->udn, upnp->srv, json);

	if (json == NULL)
		return 0;

	upnp_luv_t *ul = (upnp_luv_t *)zalloc(sizeof(upnp_luv_t));
	ul->in = strdup(json);

	static uv_work_t w;
	w.data = ul;
	uv_queue_work(upnp->loop, &w, upnp_notify_thread, upnp_notify_done);

	return 0;
}

// upnp_start()
static int upnp_start(lua_State *L) {
	pthread_t tid;
	pthread_create(&tid, NULL, upnp_thread_start, NULL);
	return 0;
}

// upnp_stop()
static int upnp_stop(lua_State *L) {
	pthread_t tid;
	pthread_create(&tid, NULL, upnp_thread_stop, NULL);
	return 0;
}
	
void upnp_init(lua_State *L, uv_loop_t *loop) {
	upnp->L = L;
	upnp->loop = loop;

	// upnp_notify = [native function]
	lua_pushcfunction(L, upnp_notify);
	lua_setglobal(L, "upnp_notify");

	// upnp_start = [native function]
	lua_pushcfunction(L, upnp_start);
	lua_setglobal(L, "upnp_start");

	// upnp_stop = [native function]
	lua_pushcfunction(L, upnp_stop);
	lua_setglobal(L, "upnp_stop");
}

