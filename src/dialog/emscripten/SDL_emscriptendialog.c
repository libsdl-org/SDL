#include "../SDL_dialog.h"
#include "SDL_internal.h"

#include <emscripten/emscripten.h>

EMSCRIPTEN_KEEPALIVE void SDL_Emscripten_OnFileDialogComplete(SDL_DialogFileCallback callback, void *userdata, const char *const *filelist, int filter)
{
    callback(userdata, filelist, filter);
}

EMSCRIPTEN_KEEPALIVE void SDL_Emscripten_OnFileDialogCancel(SDL_DialogFileCallback callback, void *userdata, int filter)
{
    const char *const filelist[] = { NULL };
    callback(userdata, filelist, filter);
}

EMSCRIPTEN_KEEPALIVE void SDL_Emscripten_OnFileDialogError(SDL_DialogFileCallback callback, void *userdata, int filter)
{
    callback(userdata, NULL, filter);
}

void SDL_SYS_ShowFileDialogWithProperties(SDL_FileDialogType type, SDL_DialogFileCallback callback, void *userdata, SDL_PropertiesID props)
{
    if (type == SDL_FILEDIALOG_OPENFOLDER || type == SDL_FILEDIALOG_SAVEFILE) {
        SDL_Unsupported();
        callback(userdata, NULL, -1);
        return;
    }

    SDL_DialogFileFilter *filters = SDL_GetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, NULL);
    int nfilters = (int)SDL_GetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, 0);
    bool allow_many = SDL_GetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, false);

    /* *INDENT-OFF* */ // clang-format off
    MAIN_THREAD_EM_ASM({
        var SDL3 = Module['SDL3'];
        var callback = $0;
        var userdata = $1;
        var c_filters = $2;
        var nfilters = $3;
        var allow_many = $4;

        var filter = "";
        if (nfilters > 0 && c_filters !== 0) {
            for (var i = 0; i < nfilters; ++i) {
                var filter_ptr = c_filters + i * 8;
                var c_name = HEAP32[filter_ptr >> 2];
                var c_pattern = HEAP32[(filter_ptr + 4) >> 2];
                var name = c_name ? UTF8ToString(c_name) : "";
                var pattern = c_pattern ? UTF8ToString(c_pattern) : "";
                if (pattern && pattern != "*") {
                    var extensions = pattern.split(';');
                    for (var j = 0; j < extensions.length; ++j) {
                        if (filter != "") {
                            filter += ", ";
                        }
                        filter += "." + extensions[j];
                    }
                }
            }
        }

        var input = document.createElement("input");
        input.type = "file";
        input.multiple = !!allow_many;
        input.accept = filter;

        input.oncancel = function() {
            input.oncancel = null;
            input.onchange = null;
            _SDL_Emscripten_OnFileDialogCancel(callback, userdata, -1);
        };
        input.onchange = function() {
            input.oncancel = null;
            input.onchange = null;

            var files = Array.from(input.files);
            if (files.length == 0) {
                _SDL_Emscripten_OnFileDialogCancel(callback, userdata, -1);
                return;
            }

            var c_fs_filepath_arr = _malloc((files.length + 1) * 4);
            if (!c_fs_filepath_arr) {
                _SDL_Emscripten_OnFileDialogError(callback, userdata, -1);
                return;
            }
            for (var i = 0; i <= files.length; ++i) {
                HEAP32[(c_fs_filepath_arr >> 2) + i] = 0;
            }
            function releaseFilepaths() {
                for (var i = 0; i < files.length; ++i) {
                    var c_fs_filepath = HEAP32[(c_fs_filepath_arr >> 2) + i];
                    if (c_fs_filepath) {
                        _Emscripten_force_free(c_fs_filepath);
                    }
                }
                _Emscripten_force_free(c_fs_filepath_arr);
            }

            var fs_tmpdir = "/tmp/filedialog";
            try { FS.mkdir(fs_tmpdir); } catch (e) {}

            var file_promises = [];
            for (var i = 0; i < files.length; ++i) {
                file_promises.push(files[i].arrayBuffer());
            }

            Promise.all(file_promises).then(function(file_buffers) {
                for (var i = 0; i < file_buffers.length; ++i) {
                    if (!SDL3.dialog_count) {
                        SDL3.dialog_count = 0;
                    }
                    var fs_dialogdir = fs_tmpdir + "/" + SDL3.dialog_count;
                    SDL3.dialog_count += 1;

                    var fs_filepath = fs_dialogdir + "/" + files[i].name;
                    var c_fs_filepath = stringToNewUTF8(fs_filepath);
                    var uint8_array = new Uint8Array(file_buffers[i]);

                    FS.mkdir(fs_dialogdir);
                    var stream = FS.open(fs_filepath, "w");
                    FS.write(stream, uint8_array, 0, uint8_array.length, 0);
                    FS.close(stream);
                    HEAP32[(c_fs_filepath_arr >> 2) + i] = c_fs_filepath;
                }
                _SDL_Emscripten_OnFileDialogComplete(callback, userdata, c_fs_filepath_arr, -1);
                releaseFilepaths();
            }).catch(function() {
                _SDL_Emscripten_OnFileDialogError(callback, userdata, -1);
                releaseFilepaths();
            });
        };

        input.click();
    }, callback, userdata, filters, nfilters, allow_many);
    /* *INDENT-ON* */ // clang-format on
}
