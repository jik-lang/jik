#include "writer.h"

#include <assert.h>

#include "ast.h"
#include "utils.h"

void
jik_writer_init(JikWriter *cw)
{
    cw->includes      = char_buffer_new("");
    cw->functions     = char_buffer_new("");
    cw->curr_buf      = cw->includes;
    cw->finalized_buf = char_buffer_new("");
    cw->indent_repr   = "    ";
    cw->indent_level  = 0;
}

void
jik_writer_set_buffer_functions(JikWriter *cw)
{
    cw->curr_buf = cw->functions;
}

void
jik_writer_set_buffer_includes(JikWriter *cw)
{
    cw->curr_buf = cw->includes;
}

void
jik_writer_write_indent(JikWriter *cw)
{
    for (size_t i = 0; i < cw->indent_level; i++)
        char_buffer_append(cw->curr_buf, cw->indent_repr);
}

void
jik_writer_indent(JikWriter *cw)
{
    cw->indent_level++;
}

void
jik_writer_dedent(JikWriter *cw)
{
    assert(cw->indent_level > 0);
    cw->indent_level--;
}

void
jik_writer_write_line(JikWriter *cw, const char *code)
{
    jik_writer_write_indent(cw);
    char_buffer_append(cw->curr_buf, code);
    char_buffer_append(cw->curr_buf, "\n");
}

void
jik_writer_write(JikWriter *cw, const char *code)
{
    jik_writer_write_indent(cw);
    char_buffer_append(cw->curr_buf, code);
}

void
jik_writer_blank_line(JikWriter *cw)
{
    char_buffer_append(cw->curr_buf, "\n");
}

void
jik_writer_begin_block(JikWriter *cw, const char *header)
{
    jik_writer_write_line(cw, header);
    jik_writer_indent(cw);
}

void
jik_writer_end_block(JikWriter *cw)
{
    jik_writer_dedent(cw);
    jik_writer_write_line(cw, "}");
}

void
jik_writer_write_section(JikWriter *cw, char *section_name)
{
    char_buffer_append(cw->curr_buf, JIK_STRING_NCAT("\n", SECTION_LINE, "\n"));
    char_buffer_append(cw->curr_buf, JIK_STRING_NCAT("//     ", section_name, "\n"));
    char_buffer_append(cw->curr_buf, jik_string_cat(SECTION_LINE, "\n"));
}
