/* Goxel 3D voxels editor
 *
 * copyright (c) 2015 Guillaume Chereau <guillaume@noctua-software.com>
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
#include <errno.h> // IWYU pragma: keep.

#include "utils/ini.h"

static int shortcut_callback(action_t *action, void *user)
{
    if (!(action->flags & ACTION_CAN_EDIT_SHORTCUT)) return 0;
    gui_push_id(action->id);
    if (action->help)
        gui_text("%s: %s", action->id, tr(action->help));
    else
        gui_text("%s", action->id);
    gui_next_column();
    // XXX: need to check if the inputs are valid!
    gui_input_text("", action->shortcut, sizeof(action->shortcut));
    gui_next_column();
    gui_pop_id();
    return 0;
}


int gui_settings_popup(void *data)
{
    const char *names[128];
    theme_t *theme;
    int i, nb, current;
    theme_t *themes = theme_get_list();
    int ret = 0;
    const tr_lang_t *language;
    const tr_lang_t *languages;

    if (gui_section_begin(_(LANGUAGE), GUI_SECTION_COLLAPSABLE)) {
        language = tr_get_language();
        if (gui_combo_begin("##lang", language->name)) {
            languages = tr_get_supported_languages();
            for (i = 0; languages[i].id; i++) {
                if (gui_combo_item(languages[i].name,
                            &languages[i] == language)) {
                    // Note: we don't change the language yet, we do it in
                    // goxel_iter so not to mess up the UI render.
                    goxel.lang = languages[i].id;
                    settings_save();
                }
            }
            gui_combo_end();
        }
    } gui_section_end();

    if (gui_section_begin(_(THEME), GUI_SECTION_COLLAPSABLE)) {
        DL_COUNT(themes, theme, nb);
        i = 0;
        DL_FOREACH(themes, theme) {
            if (strcmp(theme->name, theme_get()->name) == 0) current = i;
            names[i++] = theme->name;
        }
        if (gui_combo("##themes", &current, names, nb)) {
            theme_set(names[current]);
            settings_save();
        }
    } gui_section_end();


    if (gui_section_begin(_(PATHS), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_text("Palettes: %s/palettes", sys_get_user_dir());
        gui_text("Progs: %s/progs", sys_get_user_dir());
    } gui_section_end();

    if (gui_section_begin(_(SHORTCUTS), GUI_SECTION_COLLAPSABLE_CLOSED)) {
        gui_columns(2);
        gui_separator();
        actions_iter(shortcut_callback, NULL);
        gui_separator();
        gui_columns(1);
    } gui_section_end();

    gui_popup_bottom_begin();
    ret = gui_button(_(OK), 0, 0);
    gui_popup_bottom_end();
    return ret;
}

static int settings_ini_handler(void *user, const char *section,
                                const char *name, const char *value,
                                int lineno)
{
    action_t *a;
    if (strcmp(section, "ui") == 0) {
        if (strcmp(name, "theme") == 0) {
            theme_set(value);
        }
        if (strcmp(name, "language") == 0) {
            tr_set_language(value);
            goxel.lang = tr_get_language()->id;
        }
    }
    if (strcmp(section, "shortcuts") == 0) {
        a = action_get_by_name(name);
        if (a) {
            strncpy(a->shortcut, value, sizeof(a->shortcut) - 1);
        } else {
            LOG_W("Cannot set shortcut for unknown action '%s'", name);
        }
    }
    return 0;
}

void settings_load(void)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/settings.ini", sys_get_user_dir());
    LOG_I("Read settings file: %s", path);
    ini_parse(path, settings_ini_handler, NULL);
}

static int shortcut_save_callback(action_t *a, void *user)
{
    FILE *file = user;
    if (strcmp(a->shortcut, a->default_shortcut ?: "") != 0)
        fprintf(file, "%s=%s\n", a->id, a->shortcut);
    return 0;
}

void settings_save(void)
{
    char path[1024];
    FILE *file;

    snprintf(path, sizeof(path), "%s/settings.ini", sys_get_user_dir());
    LOG_I("Save settings to %s", path);
    sys_make_dir(path);
    file = fopen(path, "w");
    if (!file) {
        LOG_E("Cannot save settings to %s: %s", path, strerror(errno));
        return;
    }
    fprintf(file, "[ui]\n");
    fprintf(file, "theme=%s\n", theme_get()->name);
    fprintf(file, "language=%s\n", goxel.lang);

    fprintf(file, "[shortcuts]\n");
    actions_iter(shortcut_save_callback, file);

    fclose(file);
}
