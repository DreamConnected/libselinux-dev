#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <log/log.h>
#include <selinux/android.h>
#include <selinux/label.h>

#include "android_internal.h"
#include "callbacks.h"

#ifdef __ANDROID__
#include <libaudit.h>
#endif

#ifdef __ANDROID_VNDK__
#ifndef LOG_EVENT_STRING
#define LOG_EVENT_STRING(...)
#endif  // LOG_EVENT_STRING
#endif  // __ANDROID_VNDK__

static const path_alts_t service_context_paths = { .paths = {
	{
		"/system/etc/selinux/plat_service_contexts",
		"/plat_service_contexts"
	},
	{
		"/system_ext/etc/selinux/system_ext_service_contexts",
		"/system_ext_service_contexts"
	},
	{
		"/product/etc/selinux/product_service_contexts",
		"/product_service_contexts"
	},
	{
		"/vendor/etc/selinux/vendor_service_contexts",
		"/vendor_service_contexts"
	},
	{
		"/odm/etc/selinux/odm_service_contexts",
	}
}};

static const path_alts_t hwservice_context_paths = { .paths = {
	{
		"/system/etc/selinux/plat_hwservice_contexts",
		"/plat_hwservice_contexts"
	},
	{
		"/system_ext/etc/selinux/system_ext_hwservice_contexts",
		"/system_ext_hwservice_contexts"
	},
	{
		"/product/etc/selinux/product_hwservice_contexts",
		"/product_hwservice_contexts"
	},
	{
		"/vendor/etc/selinux/vendor_hwservice_contexts",
		"/vendor_hwservice_contexts"
	},
	{
		"/odm/etc/selinux/odm_hwservice_contexts",
		"/odm_hwservice_contexts"
	},
}};

static const path_alts_t vndservice_context_paths = { .paths = {
	{
		"/vendor/etc/selinux/vndservice_contexts",
		"/vndservice_contexts"
	}
}};

static const path_alts_t keystore2_context_paths = { .paths = {
	{
		"/system/etc/selinux/plat_keystore2_key_contexts",
		"/plat_keystore2_key_contexts"
	},
	{
		"/system_ext/etc/selinux/system_ext_keystore2_key_contexts",
		"/system_ext_keystore2_key_contexts"
	},
	{
		"/product/etc/selinux/product_keystore2_key_contexts",
		"/product_keystore2_key_contexts"
	},
	{
		"/vendor/etc/selinux/vendor_keystore2_key_contexts",
		"/vendor_keystore2_key_contexts"
	}
}};

size_t find_existing_files(
		const path_alts_t *path_sets,
		const char* paths[MAX_CONTEXT_PATHS])
{
	return find_existing_files_with_partitions(
		path_sets,
		paths,
		NULL
	);
}

size_t find_existing_files_with_partitions(
		const path_alts_t *path_sets,
		const char* paths[MAX_CONTEXT_PATHS],
		const char* partitions[MAX_CONTEXT_PATHS])
{
	size_t i, j, len = 0;
	for (i = 0; i < MAX_CONTEXT_PATHS; i++) {
		for (j = 0; j < MAX_ALT_CONTEXT_PATHS; j++) {
			const char* file = path_sets->paths[i][j];
			if (file && access(file, R_OK) != -1) {
				if (partitions) {
					partitions[len] = path_sets->partitions[i];
				}
				paths[len++] = file;
				/* Within each set, only the first valid entry is used */
				break;
			}
		}
	}
	return len;
}

void paths_to_opts(const char* paths[MAX_CONTEXT_PATHS],
		size_t npaths,
		struct selinux_opt* const opts)
{
	for (size_t i = 0; i < npaths; i++) {
		opts[i].type = SELABEL_OPT_PATH;
		opts[i].value = paths[i];
	}
}

struct selabel_handle* initialize_backend(
		unsigned int backend,
		const char* name,
		const struct selinux_opt* opts,
		size_t nopts)
{
		struct selabel_handle* sehandle;

		sehandle = selabel_open(backend, opts, nopts);

		if (!sehandle) {
				selinux_log(SELINUX_ERROR, "%s: Error getting %s handle (%s)\n",
								__FUNCTION__, name, strerror(errno));
				return NULL;
		}
		selinux_log(SELINUX_INFO, "SELinux: Loaded %s context from:\n", name);
		for (unsigned i = 0; i < nopts; i++) {
			if (opts[i].type == SELABEL_OPT_PATH)
				selinux_log(SELINUX_INFO, "		%s\n", opts[i].value);
		}
		return sehandle;
}

struct selabel_handle* context_handle(
		unsigned int backend,
		const path_alts_t *context_paths,
		const char *name)
{
	const char* existing_paths[MAX_CONTEXT_PATHS];
	struct selinux_opt opts[MAX_CONTEXT_PATHS];
	int size = 0;

	size = find_existing_files(context_paths, existing_paths);
	paths_to_opts(existing_paths, size, opts);

	return initialize_backend(backend, name, opts, size);
}

struct selabel_handle* selinux_android_service_context_handle(void)
{
	return context_handle(SELABEL_CTX_ANDROID_SERVICE, &service_context_paths, "service");
}

struct selabel_handle* selinux_android_hw_service_context_handle(void)
{
	return context_handle(SELABEL_CTX_ANDROID_SERVICE, &hwservice_context_paths, "hwservice");
}

struct selabel_handle* selinux_android_vendor_service_context_handle(void)
{
	return context_handle(SELABEL_CTX_ANDROID_SERVICE, &vndservice_context_paths, "vndservice");
}

struct selabel_handle* selinux_android_keystore2_key_context_handle(void)
{
	return context_handle(SELABEL_CTX_ANDROID_KEYSTORE2_KEY, &keystore2_context_paths, "keystore2");
}

static int translate_priority(int type) {
	switch(type) {
	case SELINUX_WARNING:
		return ANDROID_LOG_WARN;
	case SELINUX_INFO:
		return ANDROID_LOG_INFO;
	default:
		return ANDROID_LOG_ERROR;
	}
}

#ifdef __ANDROID__
/* File descriptor for the audit netlink socket */
static int audit_netlink_fd = -1;

int avc_audit_netlink_open() {
	audit_netlink_fd = audit_open();
	return audit_netlink_fd;
}

void avc_audit_netlink_close() {
	if (audit_netlink_fd >= 0)
		audit_close(audit_netlink_fd);
	audit_netlink_fd = -1;
}
#endif

#define MAIN_LOG      (1 << 0)
#define EVENT_LOG     (1 << 1)
#define AUDIT_NETLINK (1 << 2)

static void __selinux_log_callback(int flags, int type, const char *fmt, va_list ap) {
	char *strp;

	int len = vasprintf(&strp, fmt, ap);
	if (len < 0) {
		return;
	}

	/* libselinux log messages usually contain a new line character, while
	 * Android LOG() does not expect it. Remove it to avoid empty lines in
	 * the log buffers.
	 */
	if (len > 0 && strp[len - 1] == '\n') {
		strp[len - 1] = '\0';
	}

#ifdef __ANDROID__
	if (type == SELINUX_AVC && flags & AUDIT_NETLINK) {
		if (audit_netlink_fd == -1) {
			ALOGE("selinux_log_netlink_callback was called but audit_netlink_fd = -1."
			      "avc_audit_netlink_open must be called beforehand.");
			free(strp);
			return;
		}
		int ret = audit_log_android_avc_message(audit_netlink_fd, strp);
		if (ret < 0) {
			ALOGE("audit_log_android_avc_message failed with error: %d", ret);
		}
	} else {
#else
	{
#endif
		int priority = translate_priority(type);

		LOG_PRI(priority, "SELinux", "%s", strp);
		if (flags & EVENT_LOG) {
			LOG_EVENT_STRING(AUDITD_LOG_TAG, strp);
		}
	}
	free(strp);
}

int selinux_log_callback(int type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__selinux_log_callback(MAIN_LOG | EVENT_LOG, type, fmt, ap);
	va_end(ap);
	return 0;
}

int selinux_vendor_log_callback(int type, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	__selinux_log_callback(MAIN_LOG, type, fmt, ap);
	va_end(ap);
	return 0;
}

int selinux_log_netlink_callback(int type, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	__selinux_log_callback(MAIN_LOG | EVENT_LOG | AUDIT_NETLINK, type, fmt, ap);
	va_end(ap);
	return 0;
}
