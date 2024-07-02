/* Goxel 3D voxels editor
 *
 * copyright (c) 2017 Guillaume Chereau <guillaume@noctua-software.com>
 *
 * Goxel is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.

 * Goxel is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.

 * You should have received a copy of the GNU General Public License along with
 * goxel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "goxel.h"

enum {
    DRAG_RESIZE,
    DRAG_MOVE,
};

static int g_drag_mode = 0;

typedef struct {
    tool_t  tool;

    int     snap_face;
    float   start_rect[4][4];
} tool_selection_t;

static void get_rect(const float pos[3], const float normal[3],
                     float out[4][4])
{
    plane_from_normal(out, pos, normal);
    mat4_iscale(out, 0.5, 0.5, 0);
}

static int on_hover(gesture3d_t *gest)
{
    float rect[4][4];
    uint8_t rect_color[4] = {255, 255, 0, 255};

    if (gest->snaped & (SNAP_SELECTION_OUT | SNAP_SELECTION_IN)) {
        return -1;
    }
    goxel_set_help_text("Click and drag to set selection.");
    get_rect(gest->pos, gest->normal, rect);
    render_box(&goxel.rend, rect, rect_color, EFFECT_WIREFRAME);
    return 0;
}

static int on_drag(gesture3d_t *gest)
{
    tool_selection_t *tool = gest->user;
    float rect[4][4];
    float p[3];
    int dir;

    goxel_set_help_text("Drag.");

    get_rect(gest->pos, gest->normal, rect);
    if (gest->state == GESTURE3D_STATE_BEGIN)
        mat4_copy(rect, tool->start_rect);

    box_union(tool->start_rect, rect, goxel.selection);
    // If the selection is flat, we grow it one voxel.
    if (box_get_volume(goxel.selection) == 0) {
        dir = gest->snaped == SNAP_VOLUME ? -1 : 1;
        vec3_addk(gest->pos, gest->normal, dir, p);
        bbox_extends_from_points(goxel.selection, 1, &p, goxel.selection);
    }
    return 0;
}

// XXX: this is very close to tool_shape_iter.
static int iter(tool_t *tool, const painter_t *painter,
                const float viewport[4])
{
    float transf[4][4];
    tool_selection_t *selection = (tool_selection_t*)tool;
    int snap_mask = goxel.snap_mask;

    // To cleanup.
    snap_mask |= SNAP_ROUNDED;
    snap_mask &= ~(SNAP_SELECTION_IN | SNAP_SELECTION_OUT);

    if (box_edit(goxel.selection, g_drag_mode == DRAG_RESIZE ? 1 : 0,
                 transf, NULL)) {
        mat4_mul(transf, goxel.selection, goxel.selection);
        return 0;
    }

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_HOVER,
        .snap_mask = snap_mask | SNAP_SELECTION_OUT,
        .callback = on_hover,
        .user = selection,
    });

    goxel_gesture3d(&(gesture3d_t) {
        .type = GESTURE3D_TYPE_DRAG,
        .snap_mask = snap_mask & ~(SNAP_SELECTION_IN | SNAP_SELECTION_OUT),
        .callback = on_drag,
        .user = selection,
    });

    return tool->state;
}

static float get_magnitude(float box[4][4], int axis_index)
{
    return box[0][axis_index] + box[1][axis_index] + box[2][axis_index];
}

static int gui(tool_t *tool)
{
    float x_mag, y_mag, z_mag;
    int x, y, z, w, h, d;
    float (*box)[4][4] = &goxel.selection;
    if (box_is_null(*box)) return 0;

    gui_text("Drag mode");
    gui_combo("##drag_mode", &g_drag_mode,
              (const char*[]) {"Resize", "Move"}, 2);

    gui_group_begin(NULL);
    if (gui_action_button(ACTION_reset_selection, "Reset", 1.0)) {
        gui_group_end();
        return 0;
    }
    gui_action_button(ACTION_fill_selection, "Fill", 1.0);
    gui_row_begin(2);
    gui_action_button(ACTION_add_selection, "Add", 0.5);
    gui_action_button(ACTION_sub_selection, "Sub", 1.0);
    gui_row_end();
    gui_action_button(ACTION_cut_as_new_layer, "Cut as new layer", 1.0);
    gui_group_end();

    // XXX: why not using gui_bbox here?
    x_mag = fabs(get_magnitude(*box, 0));
    y_mag = fabs(get_magnitude(*box, 1));
    z_mag = fabs(get_magnitude(*box, 2));
    w = round(x_mag * 2);
    h = round(y_mag * 2);
    d = round(z_mag * 2);
    x = round((*box)[3][0] - x_mag);
    y = round((*box)[3][1] - y_mag);
    z = round((*box)[3][2] - z_mag);

    gui_group_begin("Origin");
    gui_input_int("x", &x, 0, 0);
    gui_input_int("y", &y, 0, 0);
    gui_input_int("z", &z, 0, 0);
    gui_group_end();

    gui_group_begin("Size");
    gui_input_int("w", &w, 0, 0);
    gui_input_int("h", &h, 0, 0);
    gui_input_int("d", &d, 0, 0);
    w = max(1, w);
    h = max(1, h);
    d = max(1, d);
    gui_group_end();

    bbox_from_extents(*box,
            VEC(x + w / 2., y + h / 2., z + d / 2.),
            w / 2., h / 2., d / 2.);
    return 0;
}

TOOL_REGISTER(TOOL_SELECTION, selection, tool_selection_t,
              .name = STR_SELECTION,
              .iter_fn = iter,
              .gui_fn = gui,
              .default_shortcut = "R",
              .flags = TOOL_SHOW_MASK,
)
