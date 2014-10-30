#include <time.h>
#include <errno.h>
#include <lua.h>
#include <uv.h>

#include "utils.h"
#include "alarm.h"

#include <linux/ioctl.h>
#include <time.h>

enum android_alarm_type {
	/* return code bit numbers or set alarm arg */
	ANDROID_ALARM_RTC_WAKEUP,
	ANDROID_ALARM_RTC,
	ANDROID_ALARM_ELAPSED_REALTIME_WAKEUP,
	ANDROID_ALARM_ELAPSED_REALTIME,
	ANDROID_ALARM_SYSTEMTIME,

	ANDROID_ALARM_TYPE_COUNT,

	/* return code bit numbers */
	/* ANDROID_ALARM_TIME_CHANGE = 16 */
};

/* Disable alarm */
#define ANDROID_ALARM_CLEAR(type)           _IO('a', 0 | ((type) << 4))

/* Ack last alarm and wait for next */
#define ANDROID_ALARM_WAIT                  _IO('a', 1)

#define ALARM_IOW(c, type, size)            _IOW('a', (c) | ((type) << 4), size)
#define ANDROID_ALARM_SET(type)             ALARM_IOW(2, type, struct timespec)

typedef struct alarm_s {
    lua_State *L;
    uv_loop_t *loop;
    uv_work_t *work_alarm;
    int alarm_fd;

    int hour;
    int minute;
    int dayofweek;
    int done;
} alarm_t;

static struct alarm_s _alarm, *alarm = &_alarm; 

enum {
    ALARM_NONE      = 0,
    ALARM_MONDAY    = (0x1),
    ALARM_TUESDAY   = (0x1 << 1),
    ALARM_WEDNESDAY = (0x1 << 2),
    ALARM_THUSDAY   = (0x1 << 3),
    ALARM_FRIDAY    = (0x1 << 4),
    ALARM_SATURDAY  = (0x1 << 5),
    ALARM_SUNDAY    = (0x1 << 6),
};

static int alarm_isdayset(const int alarm_dayweek, const int day) {
    return ((alarm_dayweek & (1 << day)) > 0);
}

static int alarm_dayofweek_cnt(const int alarm_dayweek, const int curr_dayweek) {
    if (alarm_dayweek == 0)
        return 0;

    int today = (curr_dayweek + 6) % 7;

    int day = 0;
    int daycnt = 0;
    for (; daycnt < 7; daycnt++) {
        day = (today + daycnt) % 7;
        if (alarm_isdayset(alarm_dayweek, day))
            break;
    }
    return daycnt;
}

static int alarm_get_next(struct alarm_s *alarm) {
    time_t curtime;
    time(&curtime);

    struct tm *ts = (struct tm *)zalloc(sizeof(struct tm));
    localtime_r(&curtime, ts);

    int alarm_day = alarm_dayofweek_cnt(alarm->dayofweek, ts->tm_wday);

    // if alarm is passed today
    if ((alarm_day == 0) && (alarm->dayofweek != ALARM_NONE)) {
        if ((alarm->hour < ts->tm_hour) || 
                (alarm->hour == ts->tm_hour && alarm->minute <= ts->tm_min)) {
            alarm_day += 1;
        }
    }

    debug("Alarm current time: %d:%d", ts->tm_hour, ts->tm_min);
    debug("Alarm setting time: %d:%d add %d day", alarm->hour, alarm->minute, alarm_day);

    int deltatime = alarm_day * 3600 * 24 + (alarm->hour - ts->tm_hour) * 3600 + 
            (alarm->minute - ts->tm_min) * 60 - ts->tm_sec;

    free(ts);
    return deltatime;
}

static void alarm_wait_thread(uv_work_t *w) {
    int result;
    do {
        result = ioctl(alarm->alarm_fd, ANDROID_ALARM_WAIT);
    } while (result < 0 && errno == EINTR);

    if (result < 0) {
        panic("Alarm wait error");
    }
}

static void alarm_trigger(void) {
    lua_State *L = alarm->L;

    lua_getglobal(L, "alarm_on_trigger");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_call_or_die(L, 0, 0);
}

static void alarm_wait_done(uv_work_t *w, int _) {
    info("Alarm trigger");
    alarm_trigger();
    alarm->done = 1;
}

// alarm_start(seconds)
static int lua_alarm_start(lua_State *L) {
    int type = ANDROID_ALARM_RTC_WAKEUP;
    int result;

    int alarm_sec = lua_tonumber(L, 1);
    debug("Alarm minimal: %ds", alarm_sec);
    if (alarm_sec <= 0) {
        info("Alarm cancel");
        result = ioctl(alarm->alarm_fd, ANDROID_ALARM_CLEAR(type), NULL);
        if (result < 0) {
            panic("Alarm clear type: %d", type);
        }
        return 0;
    }

    struct timespec ts;
    time(&ts.tv_sec);
    ts.tv_sec = ts.tv_sec + alarm_sec + 1;
    ts.tv_nsec = 0;

    result = ioctl(alarm->alarm_fd, ANDROID_ALARM_SET(type), &ts);
    if (result < 0) {
        panic("Alarm set type: %d", type);
    }

    info("Alarm set at %ds later", alarm_sec);

    if (alarm->done == 1) {
        uv_queue_work(alarm->loop, alarm->work_alarm, alarm_wait_thread, alarm_wait_done);
        alarm->done = 0;
    }

    return 0;
}

// next = alarm_next(hour, minute, dayofweek)
static int lua_alarm_next(lua_State *L) {
    struct alarm_s alarm;
    alarm.hour = lua_tonumber(L, 1);
    alarm.minute = lua_tonumber(L, 2);
    alarm.dayofweek = lua_tonumber(L, 3);

    debug("Alarm next: %d:%d 0x%x", alarm.hour, alarm.minute, alarm.dayofweek);
    lua_pushnumber(L, alarm_get_next(&alarm));
    return 1;
}

static int lua_alarm_init(lua_State *L) {
    info("Alarm init");
    uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));
    alarm->L = L;
    alarm->loop = loop;
    alarm->work_alarm = (uv_work_t *)zalloc(sizeof(uv_work_t));
    alarm->done = 1;

    alarm->alarm_fd = open("/dev/alarm", O_RDWR);
    if (alarm->alarm_fd < 0)
        panic("open alarm dev failed: %s", strerror(errno));

    return 0;
}

void luv_alarm_init(lua_State *L, uv_loop_t *loop) {
    lua_pushuserptr(L, loop);
    lua_pushcclosure(L, lua_alarm_init, 1);
    lua_setglobal(L, "alarm_init");

    lua_register(L, "alarm_next", lua_alarm_next);
    lua_register(L, "alarm_start", lua_alarm_start);
}
