#!/usr/bin/env python3

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

"""
This script builds the target QTVM binary.

It does so with the following steps:
1. parse build.json file
2. verify build.json contents
3. generate a Makefile
4. build project / call the Makefile
"""

# Note: This Python script file has been tested with Python v3.8.11.

import json
import sys
import argparse
import textwrap
import os
import re
import logging

# TODO: remove/upgrade esdk part
# False: using eSDK (!!!NOT SUPPORTED ON KAILUA LE!!!)
USING_STANDARD_SDK = True

# default c setting: -std=c11
DEFAULT_C_STD = 11

# WE DONT SUPPORT FLEXIBLE C STANDARDS
SUPPORT_FLEXIBLE_C_STD = False

# default c++ setting: -std=c++17
DEFAULT_CXX_STD = 17

# WE DONT SUPPORT FLEXIBLE C++ STANDARDS
SUPPORT_FLEXIBLE_CXX_STD = False

valid_flags_set = {
    "idl_dir",
    "includes",
    "sources",
    "staticLibsWholeArchive",
    "staticLibsNoWholeArchive",
    "dynamicLibs",
    "customizedCFlags",
    "customizedCXXFlags",
    "minkIDLFlags",
    "sectionMetadata",
}


def getCStd(stdStr):
    """Get the C standard version."""
    CStdPattern = re.compile(r'^\s*-std=c\d{1,2}\s*$')
    versionPattern = re.compile(r'\d{1,2}')
    if CStdPattern.match(stdStr) is not None:
        CStd = int(versionPattern.findall(stdStr)[0])
        logging.debug('Got -std=c%d' % CStd)
        if SUPPORT_FLEXIBLE_C_STD:
            raise Exception('WE DONT SUPPORT FLEXIBLE C STANDARDS!!!')
        else:
            logging.debug('We dont support flexible c standards so we ignore the c standard flag.')
        return True
    else:
        return False


def getCXXStd(stdStr):
    """Get the CXX standard version."""
    CXXStdPattern = re.compile(r'^\s*-std=c\+{2}\d{1,2}\s*$')
    versionPattern = re.compile(r'\d{1,2}')
    if CXXStdPattern.match(stdStr) is not None:
        CXXStd = int(versionPattern.findall(stdStr)[0])
        logging.debug('Got -std=c++%d' % CXXStd)
        if SUPPORT_FLEXIBLE_CXX_STD:
            raise Exception('WE DONT SUPPORT FLEXIBLE C++ STANDARDS!!!')
            DEFAULT_CXX_STD = max(DEFAULT_CXX_STD, CXXStd)
        else:
            logging.debug('We dont support flexible c++ standards so we ignore the c++ standard flag.')
        return True
    else:
        return False

# # sample build.json is as below:
# {
#     "sdk": "../../qti-distro-base-debug_sdk",
#     "includes": [
#         "./inc/"
#     ],
#     "sources": [
#         "src/main.c",
#         "src/Metadata.json"
#     ]
# }
def verify_build_json(raw_data, input_file):
    logging.debug("verifying build.json...")
    build_root = os.path.realpath(os.path.dirname(input_file))

    found_metadata = False

    if "sources" not in raw_data:
        raise Exception('should contain source file in build.json')

    for key in raw_data:
        if key not in valid_flags_set:
            logging.warning('not supported flag: %s' % key)
        elif key == "sources":
            if len(raw_data[key]) <= 0:
                raise Exception('empty source files list!')
            found_metadata = False
            for cfile in raw_data[key]:
                test_file = os.path.join(build_root, cfile)
                if not os.path.isfile(test_file):
                    raise Exception('inexistence of %s' % cfile)
                if not found_metadata:
                    found_metadata = (os.path.basename(test_file).lower() == "metadata.json")
                    metadata_json = test_file

        elif key == "includes":
            for inc_dir in raw_data[key]:
                test_dir = os.path.join(build_root, inc_dir)
                if not os.path.isdir(test_dir):
                    raise Exception('inexistence of %s(note that it should be a folder)' % inc_dir)
        elif key == "idl_dir":
            if len(raw_data[key]) > 1:
                raise Exception('Please put all your IDLs in one directory only')
            for idl_dir in raw_data[key]:
                test_dir = os.path.join(build_root, idl_dir)
                if not os.path.isdir(test_dir):
                    raise Exception('inexistence of %s(note that it should be a folder)' % inc_dir)
            raw_data[key] = os.path.realpath(test_dir)
        elif key == "staticLibsWholeArchive" or key == "staticLibsNoWholeArchive":
            for staticLib in raw_data[key]:
                test_file = os.path.join(build_root, staticLib)
                if not os.path.isfile(test_file):
                    raise Exception('inexistence of %s' % test_file)
        elif key == "customizedCFlags":
            bufCFlagsList = raw_data[key][:]
            for eachCFlag in bufCFlagsList:
                if getCStd(eachCFlag):
                    (raw_data[key]).remove(eachCFlag)
            logging.debug('The C std version is set to %d' % DEFAULT_C_STD)
        elif key == "customizedCXXFlags":
            bufCXXFlagsList = raw_data[key][:]
            for eachCXXFlag in bufCXXFlagsList:
                if getCXXStd(eachCXXFlag):
                    (raw_data[key]).remove(eachCXXFlag)
            logging.debug('The C++ std version is set to %d' % DEFAULT_CXX_STD)

    if not found_metadata:
        raise Exception("inexistence of Metadata.json!")
    else:
        metadata_dict = read_build_json(metadata_json)
        if 'appName' not in metadata_dict.keys():
            raise Exception("Key 'appName' missing from %s" % metadata_json)
        else:
            app_name = metadata_dict['appName']


    if not "idl_dir" in raw_data:
        raw_data["idl_dir"] = ""

    if not "sectionMetadata" in raw_data:
        logging.debug("metadata section not found in json. setting to False.")
        raw_data["sectionMetadata"] = False

    logging.debug('verification of build.json done')
    return raw_data, build_root, app_name

def verify_sdk_activation(sdk_root, offtarget=False):
    if not (os.path.isdir(os.path.join(sdk_root, "inc")) \
        and os.path.isfile(os.path.join(sdk_root, "SecureAppMetadata.py"))
        and os.path.isfile(os.path.join(sdk_root, "sectools"))
        and os.path.isfile(os.path.join(sdk_root, "libs/libmodule.lib"))
        and os.path.isfile(os.path.join(sdk_root, "Build.py"))):
        raise Exception("invalid sdk folder!!!")

def read_build_json(input_file):
    logging.debug('reading json file %s ...' % input_file)
    with open(input_file, 'r') as load_f:
        raw_data = json.load(load_f)
        logging.debug('read of json file done')
        return raw_data

PATH_VAR_SET_FORMAT_STRING = '''
#-------------------------------------------------------------------------
# SETUP FOR VARIABLE WITHIN OFF-LE OR STANDARD_SDK ENVIRONMENT
#-------------------------------------------------------------------------
SDK_PATH = {SDK_PATH}
PROJECT_DIR = {PROJ_PATH}
'''

IDL_DIR_SET_FORMAT_STRING = '''
#-------------------------------------------------------------------------
# SETUP FOR IDL SOURCE DIR
#-------------------------------------------------------------------------
IDL_SRC_DIR    = {IDL_DIR}
'''

ONTARGET_BUILD_VARS_STRING = '''
#-------------------------------------------------------------------------
# SETUP FOR BUILD VARIABLES FOR ONTARGET
#-------------------------------------------------------------------------
OFFTARGET=false
STUB=false
DEBUG=true
'''

OFFTARGET_BUILD_VARS_STRING = '''
#-------------------------------------------------------------------------
# SETUP FOR BUILD VARIABLES FOR OFFTARGET
#-------------------------------------------------------------------------
OFFTARGET=true
STUB=true
DEBUG=true
'''

LOWEST_CXX_STANDARD_STRING = '''
#-------------------------------------------------------------------------
# SETUP FOR DEFAULT C/C++ VERSION FOR ONTARGET
#-------------------------------------------------------------------------
LOWEST_C_STD = -std=c{C_VERSION}
LOWEST_CXX_STD = -std=c++{CXX_VERSION}
'''

# link_opts = -Wl
# link_opts += -z,relro
# link_opts += -z,now
# link_opts += -z,noexecstack
# EMPTY :=
# SPACE := $(EMPTY) $(EMPTY)
# COMMA := ,
# linker_opts = $(subst $(SPACE),$(COMMA),$(link_opts))
# linker_opts += -pie
# ^ above flags are already included in $(LDFLAGS) for yocto
C_COMMON_FORMAT_STRING = '''
#-------------------------------------------------------------------------
# Auto-generated file.  Do not edit.
#-------------------------------------------------------------------------
OBJCOPY ?= objcopy # defining for off-target builds; yocto defines it.

ifeq ($(OFFTARGET), true)
sanc = -fsanitize=address -static-libasan
sanl = -fsanitize=address
cflags = -DOFFTARGET
else
MINKSOCKET_LIB = -lminksocket_vendor
FDWRAPPER_LIB  = -lfdwrapper_vendor
endif

linker_opts += $(LDFLAGS)
app_flags = -fPIC
includes = -I $(SDK_PATH)/inc/

# TODO: which of these are needed for downloadable TAs?
ifeq ($(STUB), false)
linkflags = -lvmmem -lbase -lcutils -llog -lqrtr
else
QTVM_LIBS = $(SDK_PATH)/libs/libdmabufheap.a
endif

linkflags += -ldl -rdynamic

ifneq ($(DEBUG), false)
cflags += -DDEBUG
endif

USER_DEFINED_STATIC_LIBS_WHOLE_ARCHIVED = {staticLibsWholeArchived}
USER_DEFINED_STATIC_LIBS_NO_WHOLE_ARCHIVED = {staticLibsNoWholeArchived}
USER_DEFINED_DYNAMIC_LIBS = {dynamicLibs}

CUSTOMIZED_CFLAGS = {customizedCFlags}
CUSTOMIZED_CXXFLAGS = {customizedCXXFlags}

minkidlpath = $(SDK_PATH)

OUT_DIR = bin

# Define PLATFORM inputs
IDL_SRC        = $(wildcard $(IDL_SRC_DIR)/*.idl)
IDL_INVOKE_SRC = $(wildcard $(IDL_SRC_DIR)/I*.idl)

# Define PLATFORM targets
IDL_OUT_DIR    = $(OUT_DIR)/inc
IDL_H          = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%.h,          $(IDL_SRC))
IDL_HPP        = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%.hpp,        $(IDL_SRC))
IDL_INVOKE_H   = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%_invoke.h,   $(IDL_INVOKE_SRC))
IDL_INVOKE_HPP = $(patsubst $(IDL_SRC_DIR)/%.idl, $(IDL_OUT_DIR)/%_invoke.hpp, $(IDL_INVOKE_SRC))

HEADERS := $(IDL_H) $(IDL_HPP) $(IDL_INVOKE_H) $(IDL_INVOKE_HPP)
includes := -I $(IDL_OUT_DIR) $(includes)

ME = $(shell basename $(CURDIR))

# Define METADATA txt
METADATA_TXT = $(patsubst %.json,%.txt, {METADATA_JSON})

all : $(OUT_DIR)/{APP_NAME}

$(METADATA_TXT) : {METADATA_JSON}
{tabc}python3 $(SDK_PATH)/SecureAppMetadata.py --i {METADATA_JSON} --p $(PROJECT_DIR)

$(OUT_DIR)/{APP_NAME} : $(OUT_DIR)/TMP_{APP_NAME} $(METADATA_TXT) | $(OUT_DIR)/
{tabc}chmod a+x $(SDK_PATH)/sectools
{build_recipe}

$(OUT_DIR)/ $(IDL_OUT_DIR)/ :
{tabc}mkdir -p $@

$(IDL_OUT_DIR)/%.h : $(IDL_SRC_DIR)/%.idl | $(IDL_OUT_DIR)/
	$(minkidlpath)/minkidl {minkIDLFlags} -I $(minkidlpath)/idl --marking $(minkidlpath)/marking.txt -o $@ $<
$(IDL_OUT_DIR)/%.hpp : $(IDL_SRC_DIR)/%.idl | $(IDL_OUT_DIR)/
	$(minkidlpath)/minkidl {minkIDLFlags} -I $(minkidlpath)/idl --marking $(minkidlpath)/marking.txt -o $@ $< --cpp
$(IDL_OUT_DIR)/%_invoke.h: $(IDL_SRC_DIR)/%.idl | $(IDL_OUT_DIR)/
	$(minkidlpath)/minkidl {minkIDLFlags} -I $(minkidlpath)/idl --marking $(minkidlpath)/marking.txt -o $@ $< --skel
$(IDL_OUT_DIR)/%_invoke.hpp: $(IDL_SRC_DIR)/%.idl | $(IDL_OUT_DIR)/
	$(minkidlpath)/minkidl {minkIDLFlags} -I $(minkidlpath)/idl --marking $(minkidlpath)/marking.txt -o $@ $< --skel --cpp

clean:
{tabc}rm -rf $(OUT_DIR) *.txt Makefile src/*.o $(METADATA_TXT)

install:
{tabc}

# END FLAG OF COMMON HEADER
'''

METADATA_SECTION_RECIPE = "	$(OBJCOPY) --add-section .mink_metadata=$(METADATA_TXT) --set-section-flags .mink_metadata=noload,readonly $< $@ "

METADATA_SEGMENT_RECIPE = '''	$(SDK_PATH)/sectools elf-tool generate --data $< --out $@ --flags 0x3000004 --elf-machine AARCH64
	$(SDK_PATH)/sectools elf-tool insert $@ --data $(METADATA_TXT) --out $@ --flags 0x4000004 '''

C_INC_FORMAT_STRING = '''
includes += -I {INC_DIR}
'''

# $(includes) comes before $(CPPFLAGS) because SDK includes are more likely to be error-free than version-controlled ones.
C_C_FILE_FORMAT_STRING = '''
src/{FILENAME}.o: {C_FILE} $(HEADERS)
{tabc}$(CC) -o $@ -c $(includes) $(CFLAGS) $(CUSTOMIZED_CFLAGS) $(app_flags) $(LOWEST_C_STD) $(cflags) -g $<
'''

# $(includes) comes before $(CPPFLAGS) because SDK includes are more likely to be error-free than version-controlled ones.
C_CPP_FILE_FORMAT_STRING = '''
src/{FILENAME}.o: {CPP_FILE} $(HEADERS)
{tabc}$(CXX) -o $@ -c $(includes) $(CPPFLAGS) $(CUSTOMIZED_CXXFLAGS) $(app_flags) $(LOWEST_CXX_STD) $(cflags) -g $<
'''

# Continue to use libminksocket_vendor.a and libfdwrapper_vendor.a during off-target testing.
FINAL_LINK_FORMAT_STRING = '''
$(OUT_DIR)/TMP_{APP_NAME}: {ALL_OBJECT_FILES} | $(OUT_DIR)/
{tabc}$(CXX) -o $@ $(linker_opts) -g $^ -Wl,--whole-archive $(MINKSOCKET_LIB) $(FDWRAPPER_LIB) $(SDK_PATH)/libs/libmodule.lib $(QTVM_LIBS) $(USER_DEFINED_STATIC_LIBS_WHOLE_ARCHIVED) -Wl,--no-whole-archive $(USER_DEFINED_STATIC_LIBS_NO_WHOLE_ARCHIVED) $(MINKSOCKET_LIB) $(FDWRAPPER_LIB) -pthread $(sanl) -lrt $(linkflags) $(USER_DEFINED_DYNAMIC_LIBS)
'''

def generate_makefile(project_root, build_dict, app_name):
    target_makefile = os.path.join(project_root, "Makefile")

    if os.path.isfile(target_makefile):
        logging.warning("Overwriting Makefile for %s!" % app_name)

    logging.debug("generating Makefile for %s..." % app_name)

    common_header_flag = "a"
    sdk_environ = PATH_VAR_SET_FORMAT_STRING.format(
        SDK_PATH = os.path.abspath(build_dict["sdk"]),
        PROJ_PATH = os.path.abspath(project_root)
    )

    cxxstd_flags = LOWEST_CXX_STANDARD_STRING.format(
        C_VERSION = str(DEFAULT_C_STD),
        CXX_VERSION = str(DEFAULT_CXX_STD)
    )

    if "idl_dir" in build_dict:
        idl_dir_header = IDL_DIR_SET_FORMAT_STRING.format(
            IDL_DIR = build_dict["idl_dir"]
        )

    with open(target_makefile, "w") as makefile:
        logging.debug("setting var for standard sdk into Makefile...")
        makefile.write(sdk_environ)
        logging.debug("setting lowest c/c++ std into Makefile...")
        makefile.write(cxxstd_flags)
        logging.debug("now writing idl dir - %s into Makefile..." % build_dict["idl_dir"])
        makefile.write(idl_dir_header)

    target_flags = ""
    if (build_dict["offtarget"]) == True:
        target_flags = OFFTARGET_BUILD_VARS_STRING.format()
        logging.debug("setting build variables for offtarget into Makefile...")
    else:
        target_flags = ONTARGET_BUILD_VARS_STRING.format()
        logging.debug("setting build variables for ontarget into Makefile...")

    with open(target_makefile, "a") as target_var:
        target_var.write(target_flags)


    c_file_list = []
    c_name_list = []
    cpp_file_list = []
    cpp_name_list = []
    metadata_file = ""
    metadata_c_dir = ""

    for each_file in build_dict["sources"]:
        test_file = os.path.join(project_root, each_file)
        base_file = os.path.basename(test_file)
        postfix = os.path.splitext(base_file)[-1]

        if postfix == ".c":
            c_file_list.append(each_file)
            c_name_list.append(os.path.splitext(base_file)[0])
        elif postfix == ".cpp":
            cpp_file_list.append(each_file)
            cpp_name_list.append(os.path.splitext(base_file)[0])
        elif postfix == ".json":
            if base_file == "Metadata.json":
                metadata_file = each_file
                metadata_c_dir = os.path.realpath(os.path.dirname(test_file))
            else:
                logging.debug("%s is not supported so it won't be compiled." % base_file)
        else:
            logging.debug("%s is not supported so it won't be compiled." % base_file)

    logging.debug("we would compile these *.c files: %s" % ', '.join(c_file_list))

    logging.debug("we would compile these *.cpp files: %s" % ', '.join(cpp_file_list))

    all_slibs_whole_archive = ""
    if "staticLibsWholeArchive" in build_dict:
        for each_slib in build_dict["staticLibsWholeArchive"]:
            all_slibs_whole_archive += " " + each_slib

    all_slibs_no_whole_archive = ""
    if "staticLibsNoWholeArchive" in build_dict:
        for each_slib in build_dict["staticLibsNoWholeArchive"]:
            all_slibs_no_whole_archive += " " + each_slib

    all_dlibs = ""
    if "dynamicLibs" in build_dict:
        for each_dlib in build_dict["dynamicLibs"]:
            all_dlibs += " -l" + each_dlib

    allCustomizedCFlags = ""
    if "customizedCFlags" in build_dict:
        for eachCFlag in build_dict["customizedCFlags"]:
            allCustomizedCFlags += " " + eachCFlag

    allCustomizedCXXFlags = ""
    if "customizedCXXFlags" in build_dict:
        for eachCXXFlag in build_dict["customizedCXXFlags"]:
            allCustomizedCXXFlags += " " + eachCXXFlag

    allMinkIDLFlags = ""
    if "minkIDLFlags" in build_dict:
        for eachIDLFlag in build_dict["minkIDLFlags"]:
            allMinkIDLFlags += " " + eachIDLFlag

    if build_dict["sectionMetadata"]:
        build_recipe = METADATA_SECTION_RECIPE
    else:
        build_recipe = METADATA_SEGMENT_RECIPE

    # work-around for '\n\t' not being recognized when string.format()
    tabcc = chr(9)

    common_header = C_COMMON_FORMAT_STRING.format(
        APP_NAME = app_name,
        tabc = tabcc,
        METADATA_JSON = metadata_file,
        METADATA_C = metadata_c_dir + "/metadata.c",
        staticLibsWholeArchived = all_slibs_whole_archive,
        staticLibsNoWholeArchived = all_slibs_no_whole_archive,
        dynamicLibs = all_dlibs,
        customizedCFlags = allCustomizedCFlags,
        customizedCXXFlags = allCustomizedCXXFlags,
        minkIDLFlags = allMinkIDLFlags,
        build_recipe = build_recipe,
    )

    with open(target_makefile, common_header_flag) as output_header:
        logging.debug("now writing common header into Makefile...")
        output_header.write(common_header)

    if "includes" in build_dict:
        for each_file in build_dict["includes"]:
            header_part = C_INC_FORMAT_STRING.format(
                INC_DIR = each_file
            )
            with open(target_makefile, "a") as output_inc:
                logging.debug("now writing inc dir - %s into Makefile..." % each_file)
                output_inc.write(header_part)

    all_objects = ""

    for idx in range(len(c_file_list)):
        c_file = c_file_list[idx]
        c_name = c_name_list[idx]

        all_objects += (" src/%s.o" % c_name)

        c_file_part = C_C_FILE_FORMAT_STRING.format(
            C_FILE = c_file,
            FILENAME = c_name,
            tabc = tabcc
        )

        with open(target_makefile, "a") as output_c:
            logging.debug("now writing c files\' rule - %s into Makefile..." % c_file)
            output_c.write(c_file_part)

    for idx in range(len(cpp_file_list)):
        cpp_file = cpp_file_list[idx]
        cpp_name = cpp_name_list[idx]

        all_objects += (" src/%s.o" % cpp_name)

        cpp_file_part = C_CPP_FILE_FORMAT_STRING.format(
            CPP_FILE = cpp_file,
            FILENAME = cpp_name,
            tabc = tabcc
        )

        with open(target_makefile, "a") as output_cpp:
            logging.debug("now writing cpp files\' rule - %s into Makefile..." % cpp_file)
            output_cpp.write(cpp_file_part)

    link_part = FINAL_LINK_FORMAT_STRING.format(
        ALL_OBJECT_FILES = all_objects,
        APP_NAME = app_name,
        tabc = tabcc
    )

    with open(target_makefile, "a") as output_link:
        logging.debug("now writing linking part into Makefile...")
        output_link.write(link_part)

    logging.debug("Makefile is generated and located at %s" % target_makefile)

def build_project(project_root, offtarget=False):
    project_name = os.path.basename(project_root)

    if offtarget:
        logging.debug("Starting to build %s within off-target environ..." % project_name)
    elif USING_STANDARD_SDK:
        logging.debug("start to build %s using standard sdk..." % project_name)

        # !!!ESSENTIAL SETTING FOR ON-TARGET!!!
        # The new environment will ONLY apply to python itself and all subprocesses.
        os.environ['OFFTARGET'] = 'false'
        os.environ['STUB'] = 'false'
    else:
        raise Exception('eSDK isnt supported on Kailua LE!!!')

    ret =  os.system("make -C %s" % project_root)
    logging.debug("Finished 'make'. return value = %d" % ret)
    return ret

if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog=textwrap.dedent('''\
         additional information:
            1. Supported build.json flags:
            {}

            2. sdk folder should look like:
             # sdk
             #   |__________ Build.py
             #   |__________ SecureAppMetadata.py
             #   |__________ idl/
             #   |__________ inc/
             #   |__________ libs/libmodule.lib
             #   |__________ minkidl
             #   |__________ sectools
         '''.format(", ".join(valid_flags_set))))

    parser.add_argument('-i', '--input',
                        required=True,
                        type=argparse.FileType('r', encoding='UTF-8'),
                        metavar="<build.json>",
                        help="instructions for building the executable" )

    parser.add_argument('-d', '--debug',
                        action="store_const", dest="loglevel", const=logging.DEBUG,
                        default=logging.WARNING,
                        help="Pring debug messages" )

    parser.add_argument('-v', '--verbose',
                        action="store_const", dest="loglevel", const=logging.INFO,
                        help="Print info messages" )

    parser.add_argument('-o', '--offtarget',
                        action='store_true',
                        help="build for offtarget environment" )

    args = parser.parse_args()
    logging.basicConfig(level=args.loglevel)

    raw_data = read_build_json(args.input.name)
    build_dict, project_root, app_name = verify_build_json(raw_data, args.input.name)
    if (build_dict == None):
        logging.error('read_build_json failed')
        sys.exit(1)

    logging.debug("metadata section : %s" % raw_data["sectionMetadata"])
    logging.debug("offtarget: %s" % args.offtarget)

    sdk_root = os.path.realpath(os.path.join(os.path.dirname(sys.argv[0]), "./"))
    build_dict["sdk"] = sdk_root
    build_dict["offtarget"] = args.offtarget
    verify_sdk_activation(build_dict["sdk"], args.offtarget)

    generate_makefile(project_root, build_dict, app_name)

    ret = build_project(project_root, args.offtarget)
    logging.info("finished 'build_project'. return value = %d" % ret)

    if (ret):
        ret = 3

    sys.exit(ret)
