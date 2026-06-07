#!/usr/bin/env python
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

# Note:
#   This Python script file has been tested with
#     both Python v2.7.17 and Python v3.6.9.

# TODO
# This unittest is for old codes. It might need to upgrade.

import sys
sys.path.append('../')
from SecureAppMetadata import IDLScanner
from SecureAppMetadata import SecureAppMetadataClass
import os
import unittest

#-------------------------------------------------------------------------------
#  _MetadataTest - Unit Test
#-------------------------------------------------------------------------------

class _MetadataTest(unittest.TestCase):

    def setUp(self):

        # Assuming that this Python script is located at ./sdk/
        # If you changed the location of this Python script,
        # then you might have to change the path below
        # which is the uppermost directory to be scanned.
        build_root = os.path.realpath(os.path.join(os.path.dirname(sys.argv[0]), "../"))

        idl_class_ids = IDLScanner()._generate_class_id_map(build_root)
        self.md_class = SecureAppMetadataClass(idl_class_ids)

    def tearDown(self):
        self.md_class = None

    #-------------------------------------------------------------------
    #  test_privileges
    #-------------------------------------------------------------------

    def test_privileges(self):

        # Test valid privilege.
        md = {'privileges': ['MetadataTestPrivilege']}
        self.assertEqual("p=29a", self.md_class.generate_metadata(md))

        # Test multiple privileges.
        md = {'privileges': ['MetadataTestPrivilege', 'MetadataTestAnoPrivilege']}
        self.assertEqual("p=29a,309", self.md_class.generate_metadata(md))

        # Test empty privilege.
        md = {'privileges': [] }
        self.assertEqual("", self.md_class.generate_metadata(md))

        # Test invalid privilege.
        with self.assertRaises(Exception):
            self.md_class.generate_metadata({ 'privileges': ['invalid']})

    #-------------------------------------------------------------------
    #  test_services
    #-------------------------------------------------------------------

    def test_services(self):

        # Test valid service.
        md = {'services': ['MetadataTestService']}
        self.assertEqual("s=400", self.md_class.generate_metadata(md))

        # Test multiple services.
        md = {'services': ['MetadataTestService', 'MetadataTestAnoService']}
        self.assertEqual("s=400,555", self.md_class.generate_metadata(md))

        # Test invalid service.
        with self.assertRaises(Exception):
            self.md_class.generate_metadata({ 'services': ['invalid']})

    #-------------------------------------------------------------------
    #  test_appname
    #-------------------------------------------------------------------

    def test_appname(self):

        # Test valid appName.
        md = {'appName': 'TestService'}
        self.assertEqual("n=TestService", self.md_class.generate_metadata(md))

        # Test valid appName with min length.
        md = {'appName': 'X'}
        self.assertEqual("n=X", self.md_class.generate_metadata(md))

        # Test valid appName with max length.
        md = {'appName': '1234567890123456789012345678901'}
        self.assertEqual("n=1234567890123456789012345678901", self.md_class.generate_metadata(md))

        # Test invalid appName - name too short.
        with self.assertRaises(Exception):
            self.md_class.generate_metadata({ 'appName': '' })

        # Test invalid appName - name too long.
        with self.assertRaises(Exception):
            self.md_class.generate_metadata({ 'appName': '12345678901234567890123456789012' })

#-------------------------------------------------------------------------------
#  Run Unit Test.
#-------------------------------------------------------------------------------
def run_unit_test():
    print('Now run the unit test...')
    suite = unittest.TestSuite()
    suite.addTest(_MetadataTest("test_privileges"))
    suite.addTest(_MetadataTest("test_services"))
    suite.addTest(_MetadataTest("test_appname"))
    runner = unittest.TextTestRunner()
    runner.run(suite)

if __name__ == '__main__':

    if sys.hexversion >= 0x2070000:
        run_unit_test()
    else:
        print('Need python >= 2.7 to run unit tests')
