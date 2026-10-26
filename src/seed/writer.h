#ifndef JIK_WRITER_H
#define JIK_WRITER_H

#include "charbuf.h"

#define SECTION_LINE                                                                               \
    "// -----------------------------------------------------------------------------------"

typedef struct JikWriter {
    CharBuffer *includes;
    CharBuffer *functions;

    CharBuffer *curr_buf;
    CharBuffer *finalized_buf;

    char  *indent_repr;
    size_t indent_level;
} JikWriter;

void
jik_writer_init(JikWriter *cw);
void
jik_writer_set_buffer_functions(JikWriter *cw);
void
jik_writer_set_buffer_includes(JikWriter *cw);
void
jik_writer_write_indent(JikWriter *cw);
void
jik_writer_indent(JikWriter *cw);
void
jik_writer_dedent(JikWriter *cw);
void
jik_writer_write_line(JikWriter *cw, const char *code);
void
jik_writer_write(JikWriter *cw, const char *code);
void
jik_writer_blank_line(JikWriter *cw);
void
jik_writer_begin_block(JikWriter *cw, const char *header);
void
jik_writer_end_block(JikWriter *cw);
void
jik_writer_write_section(JikWriter *cw, char *section_name);

#endif
