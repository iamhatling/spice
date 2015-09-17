/* Do some tests on memory parsing
 */

#undef NDEBUG
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>

#include <spice/macros.h>
#include "memslot.h"
#include "red-parse-qxl.h"

static int exit_code = EXIT_SUCCESS;
static const char *test_name = NULL;

static void
failure(void)
{
    assert(test_name);
    printf("Test %s FAILED!\n", test_name);
    exit_code = EXIT_FAILURE;
}

static void
test(const char *desc)
{
    test_name = desc;
    printf("Starting test %s\n", desc);
    alarm(5);
}

static inline QXLPHYSICAL
to_physical(const void *ptr)
{
    return (uintptr_t) ptr;
}

static void*
create_chunk(size_t prefix, uint32_t size, QXLDataChunk* prev, int fill)
{
    uint8_t *ptr = spice_malloc0(prefix + sizeof(QXLDataChunk) + size);
    QXLDataChunk *qxl = (QXLDataChunk *) (ptr + prefix);
    memset(&qxl->data[0], fill, size);
    qxl->data_size = size;
    qxl->prev_chunk = to_physical(prev);
    if (prev)
        prev->next_chunk = to_physical(qxl);
    return ptr;
}

int main(int argc, char **argv)
{
    RedMemSlotInfo mem_info;
    memslot_info_init(&mem_info, 1 /* groups */, 1 /* slots */, 1, 1, 0);
    memslot_info_add_slot(&mem_info, 0, 0, 0 /* delta */, 0 /* start */, ~0ul /* end */, 0 /* generation */);

    RedSurfaceCmd cmd;
    QXLSurfaceCmd qxl;

    RedCursorCmd red_cursor_cmd;
    QXLCursorCmd cursor_cmd;
    QXLCursor *cursor;
    QXLDataChunk *chunks[2];

    memset(&qxl, 0, sizeof(qxl));

    qxl.surface_id = 123;

    /* try to create a surface with no issues, should succeed */
    test("no issues");
    qxl.u.surface_create.format = SPICE_SURFACE_FMT_32_xRGB;
    qxl.u.surface_create.width = 128;
    qxl.u.surface_create.stride = 512;
    qxl.u.surface_create.height = 128;
    qxl.u.surface_create.data = to_physical(malloc(0x10000));
    if (red_get_surface_cmd(&mem_info, 0, &cmd, to_physical(&qxl)))
        failure();

    /* try to create a surface with a stride too small to fit
     * the entire width.
     * This can be used to cause buffer overflows so refuse it.
     */
    test("stride too small");
    qxl.u.surface_create.stride = 256;
    if (!red_get_surface_cmd(&mem_info, 0, &cmd, to_physical(&qxl)))
        failure();

    /* try to create a surface quite large.
     * The sizes (width and height) were chosen so the multiplication
     * using 32 bit values gives a very small value.
     * These kind of values should be refused as they will cause
     * overflows. Also the total memory for the card is not enough to
     * hold the surface so surely can't be accepted.
     */
    test("too big image");
    qxl.u.surface_create.stride = 0x08000004 * 4;
    qxl.u.surface_create.width = 0x08000004;
    qxl.u.surface_create.height = 0x40000020;
    if (!red_get_surface_cmd(&mem_info, 0, &cmd, to_physical(&qxl)))
        failure();

    /* test base cursor with no problems */
    test("base cursor command");
    memset(&cursor_cmd, 0, sizeof(cursor_cmd));
    cursor_cmd.type = QXL_CURSOR_SET;

    cursor = create_chunk(SPICE_OFFSETOF(QXLCursor, chunk), 128 * 128 * 4, NULL, 0xaa);
    cursor->header.unique = 1;
    cursor->header.width = 128;
    cursor->header.height = 128;
    cursor->data_size = 128 * 128 * 4;

    cursor_cmd.u.set.shape = to_physical(cursor);

    if (red_get_cursor_cmd(&mem_info, 0, &red_cursor_cmd, to_physical(&cursor_cmd)))
        failure();
    free(cursor);

    /* a circular list of empty chunks should not be a problems */
    test("circular empty chunks");
    memset(&cursor_cmd, 0, sizeof(cursor_cmd));
    cursor_cmd.type = QXL_CURSOR_SET;

    cursor = create_chunk(SPICE_OFFSETOF(QXLCursor, chunk), 0, NULL, 0xaa);
    cursor->header.unique = 1;
    cursor->header.width = 128;
    cursor->header.height = 128;
    cursor->data_size = 128 * 128 * 4;

    chunks[0] = create_chunk(0, 0, &cursor->chunk, 0xaa);
    chunks[0]->next_chunk = to_physical(&cursor->chunk);

    cursor_cmd.u.set.shape = to_physical(cursor);

    memset(&red_cursor_cmd, 0xaa, sizeof(red_cursor_cmd));
    if (!red_get_cursor_cmd(&mem_info, 0, &red_cursor_cmd, to_physical(&cursor_cmd))) {
        /* function does not return errors so there should be no data */
        assert(red_cursor_cmd.type == QXL_CURSOR_SET);
        assert(red_cursor_cmd.u.set.position.x == 0);
        assert(red_cursor_cmd.u.set.position.y == 0);
        assert(red_cursor_cmd.u.set.shape.data_size == 0);
    }
    free(cursor);
    free(chunks[0]);

    /* a circular list of small chunks should not be a problems */
    test("circular small chunks");
    memset(&cursor_cmd, 0, sizeof(cursor_cmd));
    cursor_cmd.type = QXL_CURSOR_SET;

    cursor = create_chunk(SPICE_OFFSETOF(QXLCursor, chunk), 1, NULL, 0xaa);
    cursor->header.unique = 1;
    cursor->header.width = 128;
    cursor->header.height = 128;
    cursor->data_size = 128 * 128 * 4;

    chunks[0] = create_chunk(0, 1, &cursor->chunk, 0xaa);
    chunks[0]->next_chunk = to_physical(&cursor->chunk);

    cursor_cmd.u.set.shape = to_physical(cursor);

    memset(&red_cursor_cmd, 0xaa, sizeof(red_cursor_cmd));
    if (!red_get_cursor_cmd(&mem_info, 0, &red_cursor_cmd, to_physical(&cursor_cmd))) {
        /* function does not return errors so there should be no data */
        assert(red_cursor_cmd.type == QXL_CURSOR_SET);
        assert(red_cursor_cmd.u.set.position.x == 0);
        assert(red_cursor_cmd.u.set.position.y == 0);
        assert(red_cursor_cmd.u.set.shape.data_size == 0);
    }
    free(cursor);
    free(chunks[0]);

    return exit_code;
}
