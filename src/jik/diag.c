#include "diag.h"

#include "vec.h"

typedef enum JikMsgType {
    JIK_DIAG_MSG_TYPE_WARNING,
    JIK_DIAG_MSG_TYPE_ERROR,
} JikMsgType;

typedef struct JikMsg {
    JikMsgType  type;
    const char *summary;
    const char *details;
} JikMsg;

#define JIK_DIAG_MSG_PREFIX "jik"

static char *JIK_DIAG_MSG_TYPE_NAMES[] = {
    [JIK_DIAG_MSG_TYPE_WARNING] = "warning",
    [JIK_DIAG_MSG_TYPE_ERROR]   = "error",
};

static void
jik_diag_msg_print(JikMsg *m)
{
    fprintf(stderr,
            "%s %s: %s\n    %s\n",
            JIK_DIAG_MSG_PREFIX,
            JIK_DIAG_MSG_TYPE_NAMES[m->type],
            m->summary,
            m->details);
}

JIK_VEC_DECLARE(VecJikMsg, JikMsg);
JIK_VEC_DEFINE(VecJikMsg, JikMsg);

typedef struct JikDiag {
    VecJikMsg *messages;
} JikDiag;

static JikDiag jik_diag;

void
jik_diag_init(void)
{
    jik_diag.messages = VecJikMsg_new_empty();
}

JIK_NORETURN void
jik_diag_fatal_error(char *summary, char *details)
{
    JikMsg m = (JikMsg){.type = JIK_DIAG_MSG_TYPE_ERROR, .summary = summary, .details = details};
    jik_diag_msg_print(&m);
    exit(EXIT_FAILURE);
}

void
jik_diag_fatal_error_if(bool cond, char *summary, char *details)
{
    if (cond) {
        jik_diag_fatal_error(summary, details);
    }
}

void
jik_diag_warning(char *summary, char *details)
{
    JikMsg m = (JikMsg){.type = JIK_DIAG_MSG_TYPE_WARNING, .summary = summary, .details = details};
    VecJikMsg_push(jik_diag.messages, m);
}

void
jik_diag_warning_if(bool cond, char *summary, char *details)
{
    if (cond) {
        jik_diag_warning(summary, details);
    }
}

void
jik_diag_messages_print(void)
{
    for (size_t i = 0; i < VecJikMsg_size(jik_diag.messages); i++) {
        jik_diag_msg_print(VecJikMsg_get_ptr(jik_diag.messages, i));
    }
}
