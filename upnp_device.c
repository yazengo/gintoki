
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
} upnp_luv_subs_t;

static void upnp_luv_act_cb(lua_State *L, void *_p) {
	upnp_luv_subs_t *act = (upnp_luv_subs_t *)_p;

	// local in = cjson.decode(in_str)
	// local out = upnp.emit_first('action', in)
	// local out_str = cjson.encode(out)

	lua_getglobal(L, "cjson");
	lua_getfield(L, -1, "encode");
	lua_remove(L, -2);

	lua_getglobal(L, "upnp");
	lua_getfield(L, -1, "emit_first");
	lua_remove(L, -2);
	lua_pushstring(L, "action");

	lua_getglobal(L, "cjson");
	lua_getfield(L, -1, "decode");
	lua_remove(L, -2);
	lua_pushstring(L, act->in);

	lua_call_or_die(L, 1, 1);
	lua_call_or_die(L, 2, 1);
	lua_call_or_die(L, 1, 1);

	char *out = (char *)lua_tostring(L, -1);
	if (out)
		act->out = strdup(out);

	lua_pop(L, 1);
}

int upnp_control_action_request(Upnp_EventType EventType, void *Event, void *Cookie) {
	struct Upnp_Action_Request *ca_event = (struct Upnp_Action_Request *)Event;

	ca_event->ErrCode = 0;
	ca_event->ActionResult = NULL;
	// ca_event->ActionName;

	char *in = SampleUtil_GetFirstDocumentItem(ca_event->ActionRequest, "params");
	info("in=%s", in);
	if (in == NULL)
		return -1;

	upnp_luv_subs_t act = {
		.udn = ca_event->DevUDN, .srv = ca_event->ServiceID,
		.in = in,
	};
	pthread_call_luv_sync(upnp->L, upnp->loop, upnp_luv_act_cb, &act);

	info("out=%s", act.out);
	if (act.out == NULL)
		return -1;

	UpnpAddToActionResponse(&ca_event->ActionResult, ca_event->ActionName,
			PLAYER_SERVICE_TYPE,
			"Result", act.out);
	free(act.out);

	ca_event->ErrCode = UPNP_E_SUCCESS;

	return 0;
}

static void upnp_luv_subs_cb(lua_State *L, void *_p) {
	upnp_luv_subs_t *p = (upnp_luv_subs_t *)_p;

	// local out = upnp.emit_first('subscribe')
	// local out_str = cjson.encode(out)

	lua_getglobal(L, "cjson");
	lua_getfield(L, -1, "encode");
	lua_remove(L, -2);

	lua_getglobal(L, "upnp");
	lua_getfield(L, -1, "emit_first");
	lua_remove(L, -2);
	lua_pushstring(L, "subscribe");

	lua_call(L, 1, 1);
	lua_call(L, 1, 1);

	p->out = strdup((char *)lua_tostring(L, -1));

	lua_pop(L, 1);
}

int upnp_subscription_request(struct Upnp_Subscription_Request *sr_event) {
	unsigned int i = 0;
	int cmp1 = 0; 
	int cmp2 = 0; 
	int ret = 0;

	// sid=urn:upnp-org:serviceId:munocontrol1 sid=uuid:3d298f70-1dd2-11b2-bba7-d2ef32984658 udn=uuid:Upnp-munoEmulator-1_0-1234567890007

	upnp_luv_subs_t subs = {
		.srv = sr_event->ServiceId,
		.udn = sr_event->UDN,
		.sid = sr_event->Sid,
	};
	info("srv=%s sid=%s udn=%s", subs.srv, subs.sid, subs.udn);
	pthread_call_luv_sync(upnp->L, upnp->loop, upnp_luv_subs_cb, &subs);
	info("json=%s", subs.out);

	const char *names[] = { "sync_data" };
	const char *vals[] = { subs.out };

	ret = UpnpAcceptSubscription(upnp->h, 
			subs.udn, subs.srv, names, vals, 1,
			subs.sid);
	free(subs.out);

	if (ret == UPNP_E_SUCCESS) {
		info("ok");
	} else {
		info("fail");
	}

	return 1;
}

int PlayerDeviceCallbackEventHandler(Upnp_EventType EventType, void *Event, void *Cookie) {
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

int PlayerDeviceStateInit(char *DescDocURL) {
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

int PlayerDeviceStart(
		char *ip, unsigned short port,
		const char *desc_doc_name, const char *web_dir_path,
		int combo) 
{
	int ret = UPNP_E_SUCCESS;
	char desc_doc_url[DESC_URL_SIZE];

	//info("init start ip=%s port=%u", ip, port);
	ret = UpnpInit(ip, port);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpInit failed: %d", ret);
		UpnpFinish();

		return ret;
	}

	ip = UpnpGetServerIpAddress();
	port = UpnpGetServerPort();
	//info("init done ip=%s port=%u", ip, port);

	if (!desc_doc_name) {
		if (combo) {
			//desc_doc_name = "tvcombodesc.xml";
		} else {
			//desc_doc_name = "munodevicedesc.xml";
			desc_doc_name = STR_DEVICE_DESC_XML;
		}
	}

	web_dir_path = getenv("UPNP_WEBROOT");
	if (web_dir_path == NULL) 
		web_dir_path = "/usr/app/web";

	snprintf(desc_doc_url, DESC_URL_SIZE, "http://%s:%d/%s", ip, port, desc_doc_name);
	//info("webroot=%s", web_dir_path);

	ret = UpnpSetWebServerRootDir(web_dir_path);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpSetWebServerRootDir('%s') failed: %d", web_dir_path, ret);
		UpnpFinish();
		return ret;
	}

	//info("desc_doc_url=%s", desc_doc_url);
	ret = UpnpRegisterRootDevice(desc_doc_url, PlayerDeviceCallbackEventHandler, &upnp->h, &upnp->h);
	if (ret != UPNP_E_SUCCESS) {
		warn("UpnpRegisterRootDevice failed: %d", ret);
		UpnpFinish();
		return ret;
	}
	
	//info("dev reg ok");
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

int PlayerDeviceStop() {
	UpnpUnRegisterRootDevice(upnp->h);
	UpnpFinish();
	SampleUtil_Finish();   

	return UPNP_E_SUCCESS;
}

// upnp.notify(table/strbuf)
static int upnp_notify(lua_State *L) {
	if (upnp->srv == NULL || upnp->udn == NULL) {
		warn("failed: upnp srv or udn == NULL");
		return 0;
	}

	// out = cjson.encode(p)
	lua_getglobal(L, "cjson");
	lua_getfield(L, -1, "encode");
	lua_remove(L, -2);
	lua_insert(L, -2);
	lua_call_or_die(L, 1, 1);

	char *json = (char *)lua_tostring(L, -1);

	const char *names[] = { "event_data" };
	const char *vals[] = { json };

	info("udn=%s srv=%s json=%s", upnp->udn, upnp->srv, json);

	int r = UpnpNotify(upnp->h, upnp->udn, upnp->srv, names, vals, 1);
	if (r == UPNP_E_SUCCESS) {
		info("ok");
	} else {
		info("fail");
	}

	lua_pop(L, 1);

	return 0;
}

static void *upnp_thread(void *_) {
	char *ip = NULL;
	char *desc_doc_name = NULL;
	char *web_dir_path = NULL;
	unsigned short port = 49152;

	PlayerDeviceStart(ip, port, desc_doc_name, web_dir_path, 0);

	return NULL;
}

void upnp_init(lua_State *L, uv_loop_t *loop) {
	upnp->L = L;
	upnp->loop = loop;

	// upnp = {}
	lua_newtable(L);
	lua_setglobal(L, "upnp");

	// emitter_init(upnp)
	lua_getglobal(L, "emitter_init");
	lua_getglobal(L, "upnp");
	lua_call_or_die(L, 1, 0);

	// upnp.notify = [native function]
	lua_getglobal(L, "upnp");
	lua_pushcfunction(L, upnp_notify);
	lua_setfield(L, -2, "notify");
	lua_pop(L, 1);

	pthread_t tid;
	pthread_create(&tid, NULL, upnp_thread, NULL);
}

