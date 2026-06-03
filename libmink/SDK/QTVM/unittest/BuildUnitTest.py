#!/usr/bin/env python3
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

# Note:
#   This Python script file has been tested with
#     both Python v3.6.9 and v2.7.17.

# TODO
# This unittest is for old codes. It might need to upgrade.

import sys
sys.path.append('../')
from Build import read_build_json
from Build import verify_build_json
from Build import verify_sdk_activation
from Build import generate_makefile
import os
import unittest
import re
import shutil

# Please set LOG_LEVEL to show your wanted message.
# LOG_LEVEL
#   0: show msg of error and debug
#   1: only show msg of error
UNITTEST_LOG_LEVEL = 0

def unittest_msg_tag(defined_level):
    if defined_level == 0:
        return "UNITTEST's Debug: "
    elif defined_level == 1:
        return "UNITTEST's Error: "

# The message-'str' is defined to be of level-'defined_level'.
def unittest_show_msg(str, defined_level, files_list=None):
    if UNITTEST_LOG_LEVEL <= defined_level:
        print(unittest_msg_tag(defined_level) + str)
        if files_list != None:
            print(files_list)

def generate_file(format_string, file_name):
    with open(file_name, "w") as file_handle:
        unittest_show_msg("generating %s..." % file_name, 0)
        file_handle.write(format_string)
        unittest_show_msg("generation of %s done." % file_name, 0)

def not_c_str(str):
    str = re.sub(r'[?"\\]', lambda m: '\\' + m.group(0), str)
    str = re.sub(r'[\x00-\x1f]', lambda m: '\\%03o' % ord(m.group(0)), str)
    return str

#-------------------------------------------------------------------------------
#  TEST INPUT FOR test_read_build_json ----- START
#-------------------------------------------------------------------------------

FULL_TRBJ = '''
{{
{tabc}"offle": "../offle",
{tabc}"sources": [
{tabc}{tabc}"src/testapp.c",
{tabc}{tabc}"src/Metadata.json"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"./inc",
{tabc}{tabc}"./inc1/"
{tabc}]
}}
'''

BRACKET_TRBJ = '''
{{
}}
'''

EMPTY_TRBJ = '''
'''

SINGLE_MAP_TRBJ = '''
{{
{tabc}"offle": "../offle",
{tabc}"sources": [
{tabc}{tabc}"src/testapp.c"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"./inc"
{tabc}]
}}
'''

NO_INCLUDES_TRBJ = '''
{{
{tabc}"offle": "../offle",
{tabc}"sources": [
{tabc}{tabc}"src/testapp.c"
{tabc}]
}}
'''

def check_no_includes_trbj(raw_data):
    if len(raw_data) != 2:
        return False
    elif "offle" not in raw_data:
        return False
    elif "sources" not in raw_data:
        return False
    elif "includes" in raw_data:
        return False
    elif raw_data["offle"] != "../offle":
        return False
    elif raw_data["sources"] != ["src/testapp.c"]:
        return False
    else:
        return True

def check_single_map_trbj(raw_data):
    if len(raw_data) != 3:
        return False
    elif "offle" not in raw_data:
        return False
    elif "sources" not in raw_data:
        return False
    elif "includes" not in raw_data:
        return False
    elif raw_data["offle"] != "../offle":
        return False
    elif raw_data["sources"] != ["src/testapp.c"]:
        return False
    elif raw_data["includes"] != ["./inc"]:
        return False
    else:
        return True

def check_full_trbj(raw_data):
    if len(raw_data) != 3:
        return False
    elif "offle" not in raw_data:
        return False
    elif "sources" not in raw_data:
        return False
    elif "includes" not in raw_data:
        return False
    elif raw_data["offle"] != "../offle":
        return False
    elif raw_data["sources"] != ["src/testapp.c", "src/Metadata.json"]:
        return False
    elif raw_data["includes"] != ["./inc", "./inc1/"]:
        return False
    else:
        return True


#-------------------------------------------------------------------------------
#  TEST INPUT FOR test_read_build_json ----- END
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#  TEST INPUT FOR test_verify_build_json ----- START
#-------------------------------------------------------------------------------

SINGLE_MAP_TVBJ = '''
{{
{tabc}"offle": "../../../offle",
{tabc}"sources": [
{tabc}{tabc}"test_for_tvbj_c.c",
{tabc}{tabc}"Metadata.json"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"test_for_tvbj_inc_0"
{tabc}]
}}
'''

FULL_MAP_TVBJ = '''
{{
{tabc}"offle": "../../../offle",
{tabc}"sources": [
{tabc}{tabc}"test_for_tvbj_c.c",
{tabc}{tabc}"test_for_tvbj_cpp.cpp",
{tabc}{tabc}"Metadata.json"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"test_for_tvbj_inc_0",
{tabc}{tabc}"test_for_tvbj_inc_1"
{tabc}]
}}
'''

WRONG_OFFLE_TVBJ = '''
{{
{tabc}"offle": "offle",
{tabc}"sources": [
{tabc}{tabc}"test_for_tvbj_c.c",
{tabc}{tabc}"test_for_tvbj_cpp.cpp",
{tabc}{tabc}"Metadata.json"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"test_for_tvbj_inc_0",
{tabc}{tabc}"test_for_tvbj_inc_1"
{tabc}]
}}
'''

C_CPP_COMMENTS = '''
// AUTO-GENERATED FILE FOR UNITTEST
'''

SAMPLE_MD = '''
{{
{tabc}"appName": "test_for_unittest_app",
{tabc}"privileges": ["TestService"]
}}
'''

#-------------------------------------------------------------------------------
#  TEST INPUT FOR test_verify_build_json ----- END
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#  TEST INPUT FOR test_generate_makefile ----- START
#-------------------------------------------------------------------------------

PREPARE_OFFLE_TGM = '''
#!/bin/bash
SCRIPT_DIR=../..
# if there are some changes in libmodule and minkd,
#   we might need to re-ship them into offle.
make -C ${SCRIPT_DIR}/../libmodule
mkdir -p ${SCRIPT_DIR}/../offle/inc
cp ${SCRIPT_DIR}/../qtvm_process.ld ${SCRIPT_DIR}/../offle/
cp ${SCRIPT_DIR}/../daemons/mink/include/*.h ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../daemons/mink/include/*.hpp ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../libminksocket/sock/*.h ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../libminksocket/sock/*.hpp ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../libminksocket/sock/include/*.h ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../libminksocket/sock/include/*.hpp ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../libminksocket/kernel/*.h ${SCRIPT_DIR}/../offle/inc/
cp ${SCRIPT_DIR}/../libminksocket/kernel/*.hpp ${SCRIPT_DIR}/../offle/inc/
# simply-copied archived lib could not be linked into binary...
# cp ${SCRIPT_DIR}/../libmodule/bin/libmodule.lib ${SCRIPT_DIR}/../offle/
rm -f ${SCRIPT_DIR}/../offle/libmodule.lib
ar -crT ${SCRIPT_DIR}/../offle/libmodule.lib ${SCRIPT_DIR}/../libmodule/bin/libmodule.lib
'''

SINGLE_MAP_BUILD_JSON_TGM = '''
{{
{tabc}"offle": "../../../../offle",
{tabc}"sources": [
{tabc}{tabc}"src_0.c",
{tabc}{tabc}"Metadata.json"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"inc_0"
{tabc}]
}}
'''

SAMPLE_MD_TGM = '''
{{
{tabc}"appName": "unittest_for_generate_makefile",
{tabc}"privileges": ["MetadataTestPrivilege", "MetadataTestAnoPrivilege"],
{tabc}"services": ["MetadataTestService", "MetadataTestAnoService"]
}}
'''

FULL_MAP_BUILD_JSON_TGM = '''
{{
{tabc}"offle": "../../../../offle",
{tabc}"sources": [
{tabc}{tabc}"src_0.c",
{tabc}{tabc}"src_1.cpp",
{tabc}{tabc}"Metadata.json"
{tabc}],
{tabc}"includes": [
{tabc}{tabc}"inc_0",
{tabc}{tabc}"inc_1"
{tabc}]
}}
'''

BASE_SRC_C = '''
// Auto-generated header file. Do not edit.
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include "object.h"
#include "inc_0_header.h"
{INC_1_OR_NOT}
#include "threadpool.h"
static sem_t shutdownLock;
void do_nonsense_work()
{{
{tabc}printf("test_val is %d in %s ", inc_0_header_test_func(234, 567), __func__);
}}
int main(int argc, char *argv[])
{{
{tabc}int ret = 0;
{tabc}ThreadWork buf_work;
{tabc}ThreadPool *buf_pool = ThreadPool_new();
{tabc}ThreadWork_init(&buf_work, do_nonsense_work, NULL);
{tabc}ThreadPool_queue(buf_pool, &buf_work);
{tabc}ThreadPool_stop(buf_pool);
{tabc}ThreadPool_release(buf_pool);
{tabc}{BUG_OR_NOT}
{tabc}{CPP_TEST}
{tabc}if (sem_init(&shutdownLock, 0, 0) != 0) {{
{tabc}{tabc}printf("Failed to initialize semaphore ");
{tabc}{tabc}return -1;
{tabc}}}
{tabc}printf("%s() ", __func__);
{tabc}for (int i = 1; i < argc; i++) {{
{tabc}{tabc}printf(" %s ", argv[i]);
{tabc}}}
{tabc}printf(" ");
{tabc}if (sem_wait(&shutdownLock) == 0) {{
{tabc}{tabc}printf("Failed to wait on semaphore ");
{tabc}{tabc}return -1;
{tabc}}}
{tabc}return ret;
}}
void tProcessShutdown(void)
{{
{tabc}printf("This is tProcessShutdown() of %s. ", __FILE__);
{tabc}sem_post(&shutdownLock);
{tabc}return;
}}
int32_t tProcessOpen(uint32_t uid, Object cred, Object *objOut)
{{
{tabc}int32_t ret = Object_OK;
{tabc}printf("[%s] Opening service with UID = %d ", __FILE__, uid);
{tabc}Object_INIT(*objOut, Object_NULL);
{tabc}return ret;
}}
'''

INC_0_HEADER = '''
// Auto-generated header file. Do not edit.
#ifndef INC_0_HEADER_H
#define INC_0_HEADER_H

int inc_0_header_test_func(int a, int b)
{{
{tabc}return (a & ((1 << 11) - 1)) ^ (b & ((1 << 11) - 1));
}}

#endif
'''

CPP_WRAPPER_PART_I_TGM = '''
#ifdef __cplusplus
extern "C" {
#endif
'''

CPP_WRAPPER_PART_II_TGM = '''
#ifdef __cplusplus
}
#endif
'''

INC_1_HEADER = '''
// Auto-generated header file. Do not edit.
#ifndef INC_1_HEADER_H
#define INC_1_HEADER_H

{CPP_WRAPPER_FLAG_I}
int inc_1_header_test_func(int, int);
{CPP_WRAPPER_FLAG_II}

#endif
'''

SRC_CPP_TGM = '''
// Auto-generated header file. Do not edit.
#include "inc_1_header.h"
int inc_1_header_test_func(int a, int b)
{{
{tabc}return (a & ((1 << 11) - 1)) | (b & ((1 << 11) - 1));
}}
'''

NO_BUG_TGM = '''
printf("test_val_0 is %d ", inc_0_header_test_func(123, 789));
'''

WITH_BUG_TGM = '''
pirntf("test_val_0 is %d ", inc_0_header_test_func(123, 789));
'''

NO_BUG_CPP_TEST_TGM = '''
printf("test_val_1 is %d ", inc_1_header_test_func(345, 789));
'''

WITH_BUG_CPP_TEST_TGM = '''
pirntf("test_val_1 is %d ", inc_1_header_test_func(345, 789));
'''

INC_1_CMD_TGM = '''
#include "inc_1_header.h"
'''

#-------------------------------------------------------------------------------
#  TEST INPUT FOR test_generate_makefile ----- END
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
#  _BuildTest - Unit Test
#-------------------------------------------------------------------------------

class _BuildTest(unittest.TestCase):

    def setUp(self):
        self.tabcc = chr(9)


    def tearDown(self):
        self.flag = False

    #-------------------------------------------------------------------
    #  test_read_build_json
    #-------------------------------------------------------------------

    def test_read_build_json(self):

        unittest_show_msg("now start to unittest read_build_json...", 0)

        test_file = "test.json"

        # In case there are some test files left after last failed test,
        #   we remove them at first.
        if os.path.isfile(test_file):
            os.remove(test_file)

        # for following formatted strings' definitions,
        #   you could reference the part of TEST INPUT FOR test_read_build_json.

        # following are all naive test cases for READing json file.

        # NEGATIVE CASES

        buf_str = FULL_TRBJ.replace("{{", "  ", 1)
        buf_str = buf_str.replace("}}", "  ", 1)
        generate_file(buf_str.format(tabc = self.tabcc), test_file)

        with self.assertRaises(Exception):
            read_build_json(test_file)

        buf_str = FULL_TRBJ.replace("\"offle\"", "offle", 1)
        generate_file(buf_str.format(tabc = self.tabcc), test_file)

        with self.assertRaises(Exception):
            read_build_json(test_file)

        generate_file(BRACKET_TRBJ, test_file)
        with self.assertRaises(Exception):
            read_build_json(test_file)

        generate_file(EMPTY_TRBJ, test_file)
        with self.assertRaises(Exception):
            read_build_json(test_file)

        buf_str = SINGLE_MAP_TRBJ.replace("[", " ", 2)
        buf_str = buf_str.replace("]", " ", 2)
        generate_file(buf_str.format(tabc = self.tabcc), test_file)
        try:
            raw_data = read_build_json(test_file)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")
        self.assertEqual(check_single_map_trbj(raw_data), False)

        buf_str = SINGLE_MAP_TRBJ.replace(",", " ", 1)
        generate_file(buf_str.format(tabc = self.tabcc), test_file)
        with self.assertRaises(Exception):
            read_build_json(test_file)

        # POSITIVE CASES

        generate_file(FULL_TRBJ.format(tabc = self.tabcc), test_file)
        try:
            raw_data = read_build_json(test_file)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")
        self.assertEqual(check_full_trbj(raw_data), True)

        generate_file(SINGLE_MAP_TRBJ.format(tabc = self.tabcc), test_file)
        try:
            raw_data = read_build_json(test_file)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")
        self.assertEqual(check_single_map_trbj(raw_data), True)

        generate_file(NO_INCLUDES_TRBJ.format(tabc = self.tabcc), test_file)
        try:
            raw_data = read_build_json(test_file)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")
        self.assertEqual(check_no_includes_trbj(raw_data), True)

        # we should not leave test files for ambiguity.
        os.remove(test_file)

        unittest_show_msg("unittest of read_build_json got okay.", 0)

    #-------------------------------------------------------------------
    #  test_verify_build_json
    #-------------------------------------------------------------------

    def test_verify_build_json(self):
        unittest_show_msg("now start to unittest verify_build_json...", 0)

        test_file = "test.json"
        test_src_0 = "test_for_tvbj_c.c"
        test_src_1 = "test_for_tvbj_cpp.cpp"
        test_inc_0 = "test_for_tvbj_inc_0"
        test_inc_1 = "test_for_tvbj_inc_1"
        test_offle = "offle"
        test_md = "Metadata.json"

        # In case there are some test files left after last failed test,
        #   we remove them at first.

        if os.path.isdir(test_inc_0):
            os.rmdir(test_inc_0)

        if os.path.isdir(test_inc_1):
            os.rmdir(test_inc_1)

        if os.path.isfile(test_src_0):
            os.remove(test_src_0)

        if os.path.isfile(test_src_1):
            os.remove(test_src_1)

        if os.path.isdir(test_offle):
            os.remove(test_offle)

        if os.path.isfile(test_file):
            os.remove(test_file)

        if os.path.isfile(test_md):
            os.remove(test_md)

        # for following formatted strings' definitions,
        #   you could reference the part of TEST INPUT FOR test_verify_build_json.

        # TEST CASES FOR single map of sources and includes
        generate_file(SINGLE_MAP_TVBJ.format(tabc = self.tabcc), test_file)

        # We assume any procedure before the test item here should be correct
        #   because we unit-test them with logically-sequential order.
        # This principle could be applied to any try-catch block later.
        try:
            raw_data = read_build_json(test_file)
        except:
            # inexistence of files inputted in build.json.
            # we havent create them yet.
            self.fail("FAILED CASE: unexpected exception raised.")

        with self.assertRaises(Exception):
            # inexistence of some file inputted in build.json.
            verify_build_json(raw_data, test_file)

        generate_file(C_CPP_COMMENTS, test_src_0)

        with self.assertRaises(Exception):
            # inexistence of some file inputted in build.json.
            verify_build_json(raw_data, test_file)

        os.mkdir(test_inc_0)

        with self.assertRaises(Exception):
            verify_build_json(raw_data, test_file)

        generate_file(SAMPLE_MD.format(tabc = self.tabcc), test_md)

        try:
            # all test files we need have been already created.
            project_input, project_root, offle = verify_build_json(raw_data, test_file)
            sdk_root = project_input["offle"]
            verify_sdk_activation(sdk_root, offle)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        # TEST CASES FOR the map mapping includes and sources to
        #   more than 1 items respectively.
        generate_file(FULL_MAP_TVBJ.format(tabc = self.tabcc), test_file)
        try:
            raw_data = read_build_json(test_file)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        with self.assertRaises(Exception):
            verify_build_json(raw_data, test_file)

        generate_file(C_CPP_COMMENTS, test_src_1)

        with self.assertRaises(Exception):
            verify_build_json(raw_data, test_file)

        os.mkdir(test_inc_1)

        try:
            project_input, project_root, offle = verify_build_json(raw_data, test_file)
            sdk_root = project_input["offle"]
            verify_sdk_activation(sdk_root, offle)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        # we now try to unittest a map with wrong offle path.
        generate_file(WRONG_OFFLE_TVBJ.format(tabc = self.tabcc), test_file)
        try:
            raw_data = read_build_json(test_file)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        os.mkdir(test_offle)

        try:
            project_input, project_root, offle = verify_build_json(raw_data, test_file)
            sdk_root = project_input["offle"]
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        with self.assertRaises(Exception):
            verify_sdk_activation(sdk_root, offle)

        # we should not leave test files for ambiguity.
        os.remove(test_file)
        os.remove(test_src_0)
        os.remove(test_src_1)
        os.rmdir(test_inc_0)
        os.rmdir(test_inc_1)
        os.rmdir(test_offle)
        os.remove(test_md)

        unittest_show_msg("unittest of verify_build_json got okay.", 0)

    #-------------------------------------------------------------------
    #  test_generate_makefile
    #-------------------------------------------------------------------

    def test_generate_makefile(self):
        unittest_show_msg("now start to unittest generate_makefile...", 0)

        test_root = "test_for_generate_makefile"
        test_inc_0 = "inc_0"
        test_inc_0_file = "inc_0_header.h"
        test_inc_1_file = "inc_1_header.h"
        test_inc_1 = "inc_1"
        test_src_0 = "src_0.c"
        test_src_1 = "src_1.cpp"

        # In case there are some test files left after last failed test,
        #   we remove them at first.

        if os.path.isdir(test_root):
            shutil.rmtree(test_root)

        # following test cases are designed to verify the practicability
        #   of compiling test files.
        # if the test files have some bugs, it should not be compiled successfully
        #   and vice versa.

        # for following formatted strings' definitions,
        #   you could reference the part of TEST INPUT FOR test_generate_makefile.

        os.mkdir(test_root)

        generate_file(PREPARE_OFFLE_TGM, test_root + "/prepare.sh")

        generate_file(SAMPLE_MD_TGM.format(tabc = self.tabcc), test_root + "/Metadata.json")

        os.mkdir(test_root + "/" + test_inc_0)

        generate_file(INC_0_HEADER.format(tabc = self.tabcc), test_root + "/" + test_inc_0 + "/" + test_inc_0_file)

        write_c = BASE_SRC_C.format(
            tabc = self.tabcc,
            BUG_OR_NOT = NO_BUG_TGM,
            CPP_TEST = not_c_str(""),
            INC_1_OR_NOT = not_c_str("")
            )

        generate_file(write_c, test_root + "/" + test_src_0)

        generate_file(SINGLE_MAP_BUILD_JSON_TGM.format(
            tabc = self.tabcc
            ), test_root + "/build.json")

        try:
            raw_data = read_build_json(test_root + "/build.json")
            project_input, project_root, offle = verify_build_json(raw_data, test_root + "/build.json")
            sdk_root = project_input["offle"]
            verify_sdk_activation(sdk_root, offle)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        generate_makefile(project_root, project_input, offle)

        # we do the below preparation for build environment only once here
        #   to ship the libraries and hdr_files we need to correct location.
        self.assertEqual(os.system("bash %s/prepare.sh" % test_root), 0)

        # no bug specified in test files and it should be compiled successfully.
        self.assertEqual(os.system("make -C %s" % project_root), 0)

        # manual 'make clean' for further trial of compiling.
        self.assertEqual(os.system("rm -rf %s/bin" % project_root), 0)

        write_c = BASE_SRC_C.format(
            tabc = self.tabcc,
            BUG_OR_NOT = WITH_BUG_TGM,
            CPP_TEST = not_c_str(""),
            INC_1_OR_NOT = not_c_str("")
            )

        generate_file(write_c, test_root + "/" + test_src_0)

        # failed reason is invoking pirntf function
        self.assertNotEqual(os.system("make -C %s" % project_root), 0)

        # manual 'make clean' for further trial of compiling.
        self.assertEqual(os.system("rm -rf %s/bin" % project_root), 0)

        # we now try to unittest test cases for
        #   simultaneous existence of C file and CPP file.

        generate_file(FULL_MAP_BUILD_JSON_TGM.format(
            tabc = self.tabcc
            ), test_root + "/build.json")

        os.mkdir(test_root + "/" + test_inc_1)

        # with no wrapper of c++ functions for c.
        generate_file(INC_1_HEADER.format(
            tabc = self.tabcc,
            CPP_WRAPPER_FLAG_I = not_c_str(""),
            CPP_WRAPPER_FLAG_II = not_c_str("")
            ), test_root + "/" + test_inc_1 + "/" + test_inc_1_file)

        generate_file(SRC_CPP_TGM.format(tabc = self.tabcc), test_root + "/" + test_src_1)

        write_c = BASE_SRC_C.format(
            tabc = self.tabcc,
            BUG_OR_NOT = NO_BUG_TGM,
            CPP_TEST = NO_BUG_CPP_TEST_TGM,
            INC_1_OR_NOT = INC_1_CMD_TGM
            )

        generate_file(write_c, test_root + "/" + test_src_0)

        try:
            raw_data = read_build_json(test_root + "/build.json")
            project_input, project_root, offle = verify_build_json(raw_data, test_root + "/build.json")
            sdk_root = project_input["offle"]
            verify_sdk_activation(sdk_root, offle)
        except:
            self.fail("FAILED CASE: unexpected exception raised.")

        generate_makefile(project_root, project_input, offle)

        # failed reason is that ld could not resolve the reference of c++ function
        #   due to inexistence of wrapper of c++ function.
        self.assertNotEqual(os.system("make -C %s" % project_root), 0)

        # manual 'make clean' for further trial of compiling.
        self.assertEqual(os.system("rm -rf %s/bin" % project_root), 0)

        # we define the wrapper of c++ function for c's invoking.
        generate_file(INC_1_HEADER.format(
            tabc = self.tabcc,
            CPP_WRAPPER_FLAG_I = CPP_WRAPPER_PART_I_TGM,
            CPP_WRAPPER_FLAG_II = CPP_WRAPPER_PART_II_TGM
            ), test_root + "/" + test_inc_1 + "/" + test_inc_1_file)

        # now ld could resolve the reference of c++ function.
        self.assertEqual(os.system("make -C %s" % project_root), 0)

        # manual 'make clean' for further trial of compiling.
        self.assertEqual(os.system("rm -rf %s/bin" % project_root), 0)

        # we make some bug here.
        write_c = BASE_SRC_C.format(
            tabc = self.tabcc,
            BUG_OR_NOT = NO_BUG_TGM,
            CPP_TEST = WITH_BUG_CPP_TEST_TGM,
            INC_1_OR_NOT = INC_1_CMD_TGM
            )

        generate_file(write_c, test_root + "/" + test_src_0)

        self.assertNotEqual(os.system("make -C %s" % project_root), 0)

        # we should not leave test files for ambiguity.
        if os.path.isdir(test_root):
            shutil.rmtree(test_root)

        unittest_show_msg("unittest of generate_makefile got okay.", 0)

def run_unit_test():
    print('Now run the unit test...')
    suite = unittest.TestSuite()
    suite.addTest(_BuildTest("test_read_build_json"))
    suite.addTest(_BuildTest("test_verify_build_json"))
    suite.addTest(_BuildTest("test_generate_makefile"))
    runner = unittest.TextTestRunner()
    runner.run(suite)

if __name__ == '__main__':

    if sys.hexversion >= 0x2070000:
        run_unit_test()

    else:
        print('Need python >= 2.7 to run unit tests')
