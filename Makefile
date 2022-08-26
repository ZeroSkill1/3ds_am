#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source source/3ds source/am
INCLUDES	:=	include include/3ds source/am

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
DEFINES :=

CFLAGS	:=	-std=gnu11 -Wall -Wextra -Werror -Wno-unused-value -flto -mword-relocations \
			-fomit-frame-pointer -ffunction-sections -fdata-sections \
			-fno-exceptions -fno-ident -fno-unwind-tables -fno-asynchronous-unwind-tables \
			-fno-tree-loop-distribute-patterns -fshort-wchar \
			$(ARCH) $(DEFINES) $(INCLUDE)

ASFLAGS	:=	$(ARCH)
LDFLAGS	=	-specs=3dsx.specs -nostartfiles -nostdlib	 \
			$(ARCH) -Wl,-Map,$(notdir $*.map)

RSF     = $(OUTPUT)_debug.rsf

ifeq ($(RELEASE),)
	CFLAGS += -g -O0
else
	CFLAGS += -Os -DREPLACE_AM
	RSF     = $(OUTPUT).rsf
endif

LIBS	:=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=  $(TOPDIR)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

# we don't use C++ here
export LD	:=	$(CC)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 		:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@echo $(CFLAGS)
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).cxi $(TARGET).elf


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all	:	$(OUTPUT).cxi

$(OUTPUT).cxi	:	$(OUTPUT).elf $(RSF)
	@makerom -f ncch -rsf $(word 2,$^) -o $@ -elf $(OUTPUT).elf
	@3dstool -xtf cxi $(OUTPUT).cxi --extendedheader $(TOPDIR)/exheader.bin --exefs $(TOPDIR)/exefs.bin
	@3dstool -xtf exefs $(TOPDIR)/exefs.bin --exefs-dir $(TOPDIR)/exefs
	@cp $(TOPDIR)/exefs/code.bin $(TOPDIR)/code.bin
	@rm $(TOPDIR)/exefs $(TOPDIR)/exefs.bin -rf
	@echo built ... $(notdir $@)

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
#---------------------------------------------------------------------------------
%.xml.o	%_xml.h:	%.xml
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
