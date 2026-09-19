LVGL_DIR := $(ROOTDIR)/lib/lvgl

INCLUDES += -I$(LVGL_DIR) -I$(LVGL_DIR)/src

LVGL_SRC := \
	$(LVGL_DIR)/src/lv_init.c \
	$(shell find $(LVGL_DIR)/src/core -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/display -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/draw -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/draw/convert -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/draw/sw -type f -name '*.c') \
	$(LVGL_DIR)/src/font/lv_font.c \
	$(LVGL_DIR)/src/font/fmt_txt/lv_font_fmt_txt.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_8.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_10.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_12.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_i18n_8.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_i18n_10.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_i18n_12.c \
	$(LVGL_DIR)/src/font/lv_font_source_han_sans_sc_14_cjk.c \
	$(LVGL_DIR)/src/font/lv_font_source_han_sans_sc_16_cjk.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_anton_16.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_anton_22.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_anton_32.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_instrument_serif_14.c \
	$(LVGL_DIR)/src/font/lv_font_crazypod_instrument_serif_28.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_16.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_24.c \
	$(LVGL_DIR)/src/font/lv_font_montserrat_48.c \
	$(LVGL_DIR)/src/font/lv_font_unscii_8.c \
	$(LVGL_DIR)/src/font/lv_font_unscii_16.c \
	$(shell find $(LVGL_DIR)/src/indev -maxdepth 1 -type f -name '*.c') \
	$(LVGL_DIR)/src/layouts/lv_layout.c \
	$(LVGL_DIR)/src/layouts/flex/lv_flex.c \
	$(LVGL_DIR)/src/layouts/grid/lv_grid.c \
	$(LVGL_DIR)/src/libs/bin_decoder/lv_bin_decoder.c \
	$(shell find $(LVGL_DIR)/src/misc -maxdepth 1 -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/misc/cache -type f -name '*.c') \
	$(LVGL_DIR)/src/osal/lv_os.c \
	$(LVGL_DIR)/src/osal/lv_os_none.c \
	$(LVGL_DIR)/src/stdlib/lv_mem.c \
	$(shell find $(LVGL_DIR)/src/stdlib/builtin -maxdepth 1 -type f -name '*.c') \
	$(LVGL_DIR)/src/themes/lv_theme.c \
	$(LVGL_DIR)/src/tick/lv_tick.c \
	$(shell find $(LVGL_DIR)/src/widgets/animimage -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/arc -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/bar -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/button -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/canvas -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/chart -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/checkbox -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/dropdown -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/image -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/imagebutton -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/label -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/list -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/roller -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/slider -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/switch -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/table -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/textarea -type f -name '*.c') \
	$(shell find $(LVGL_DIR)/src/widgets/tileview -type f -name '*.c')

SRC += $(LVGL_SRC)
OTHER_SRC += $(LVGL_SRC)

# Rockbox compiles with -Os. On the ARM7TDMI targets that costs the LVGL
# renderer real time in its hot loops (blends, masks, glyph blits, style
# lookups), so LVGL alone is built at -O2 there; the size increase lands in
# SDRAM code space, which the PortalPlayer targets have to spare.
#
# ARCH_VERSION is 4 on exactly those targets: the Video and the Mini, both
# PP502x at the same clock. It is 5 on the 6G's S5L8702 and empty in the
# simulator. Keying this on the architecture rather than on one model's
# name is deliberate -- the condition was IPOD_VIDEO, so the Mini, the same
# chip running the same renderer, silently got neither the -O2 build nor
# the perf-log instrumentation below.
ifeq ($(ARCH_VERSION),4)
LVGL_OPTFLAGS := -O2
# The perf log's LVGL columns -- render time by draw task, invalidations by
# object and caller, layer renders, the refresh phase split. The backend is
# already built for every PortalPlayer target; this compiles in the hooks
# that feed it.
LVGL_OPTFLAGS += -DCRAZYPOD_LVGL_PERF
else
LVGL_OPTFLAGS :=
endif

$(BUILDDIR)/lib/lvgl/%.o: $(ROOTDIR)/lib/lvgl/%.c $(CORE_COMPILE_GENERATED_HEADERS)
	$(SILENT)mkdir -p $(dir $@)
	$(call PRINTS,CC $(subst $(ROOTDIR)/,,$<))$(CC) $(CFLAGS) $(LVGL_OPTFLAGS) -c $< -o $@
