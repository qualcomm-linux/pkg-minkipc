#!/usr/bin/env python
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

# Note:
#   This Python script file has been tested with
#     both Python v2.7.17 and Python v3.6.9.

import copy
import os
import re
import sys
import getopt
import json

_kDefaultPrivilegeIds = { "TProcessLoader", "TMemPoolFactory" }

def _encode_id_set(iterable):
    """Encode numeric IDs as a metadata ID set value"""
    s = ''
    # The ID represented by the first bit in the next mask digit (if any)
    mask_pos = 0
    # Current value for the next mask digit to be emitted.  When non-zero,
    # we can assume that we are in mask mode (following a ':').
    mask_digit = 0

    for id in sorted(set(iterable)):

        # Each ID may be emitted as a hex number or as a mask bit.  We
        # estimate the "cost" of each in terms of bytes emitted.  The "cost"
        # of using a mask is not known entirely at this point, since
        # subsequent IDs might benefit from mask encoding, so we apply a
        # small bias toward masks.

        id_text = _hex_str(id)
        num_cost = len(id_text) + (1 if mask_digit else -1)
        mask_cost = (id - mask_pos) // 4

        if s == '' or mask_cost > num_cost:
            # Output id as a hex number.
            if mask_digit:
                s += _hex_str(mask_digit)
            if s != '':
                s += ','
            s += id_text
            mask_pos = id + 1
            mask_digit = 0              # not in a mask

        else:
            # Add id as a mask bit.
            if mask_digit == 0:
               s += ':'
            while id >= mask_pos + 4:
               s += _hex_str(mask_digit)
               mask_digit = 0
               mask_pos += 4
            mask_digit |= 1 << (id - mask_pos)
            # now:  mask_digit > 0   (in a mask)

    if mask_digit:
        s += _hex_str(mask_digit)

    return s

def _hex_str(val):
    return '%x' % (val)

def _isClassDefFile(path):
    # Looking only for file names that match C*.h (used to be C*.idl)
    name = os.path.basename(path)
    if re.match('^C.*\.h', name) and os.path.isfile(path):
      return True
    return False

def _extractIds(path):
    # Looking for lines that match something like this:
    #
    #   const uint32 CSharedBuffer_UID = 23;

    idmap = {}
    #re0 = re.compile(r'^\s*const\s+uint32\s+C(\w+)_UID\s+=\s+(0x[0-9a-fA-F]+|[0-9]+);\s*')

    # changed to look in the header file
    # #define CTMemPoolFactory_UID UINT32_C(999)

    #re0 = re.compile(r'.C(\w+)_UID\s+UINT32_C\(([0-9a-fA-F]+|[0-9]+)\)\s*')
    re0 = re.compile(r'.*\s+C(\w+)_UID\s+UINT32_C\((0x)?([0-9a-fA-F]+|[0-9]+)\)')
    with open(path) as f:
        for line in f:
            m = re0.match(line)
            if m:
                n = re0.search(line)
                # TODO: error checking to see if there are duplicate id values?
                if n:
                    id = m.group(1).lower()
                    hex_v = n.group(2)
                    if hex_v:
                        value = int(n.group(3), 16)
                    else:
                        value = int(n.group(3), 0)
                    idmap[id] = value
    return idmap

class IDLScanner:

  def __init__(self):
    self._classIdMap = {}

  def _generate_class_id_map(self, root):
    for dirName, subdirList, fileList in os.walk(root):
      for fname in fileList:
          # Check if this file is likely to hold IDL ids.
          path = os.path.join(dirName, fname)
          if not _isClassDefFile(path):
              continue
          idmap = _extractIds(path)
          # Add the ids we just found to the global id map.
          # Do some error checking along the way.
          for key in idmap:
              for key2, value in list(self._classIdMap.items()):
                  # The key is duplicate.  This is OK as long as
                  # the value is the same in both cases.
                  if key == key2:
                      print('Duplicate values : ')
                      print(key)
                      print(key2)
                      if idmap[key] != value:
                          raise Exception('Duplicate values for class id %s (%d and %d).' % ( key, value, idmap[key] ))
                      else:
                          continue
                  # The value is in both maps with a different
                  # key.  This is an error.
                  if idmap[key] == value:
                      print("found an issue with file " +
                            dirName + fname)
                      raise Exception('Duplicate definitions for value %d (%s and %s).' % ( value, key, key2 ))
              # No errors?  Add it.
              self._classIdMap[key] = idmap[key]

    return self._classIdMap

class SecureAppMetadataClass:

  def __init__(self, id_map, **kwargs):

    # Metadata field handlers.
    #    Each field name includes:
    #        - single letter abbreviation
    #        - processing function
    #        - default value if not specified
    self._metadataHandlers = {
          'privileges':    ('p', self._gen_class_set_string, set(['default'])),
          'services':      ('s', self._gen_class_set_string, None),
          'appName':       ('n', lambda val: self._check_str(r'.{1,31}', val), None),
          'neverUnload':    ('U', lambda val: self._check_boolean(val), None)
         }

    # Maps strings, e.g. "I2C", to class IDs.
    self._classIdMap = id_map
    self.metadata_current_field = None

    # The default privilege set (numbers).  This is the translation to numbers
    # of the elements in the _kDefaultPrivilegeIds set.
    self._defaultPrivileges = self._gen_class_set(_kDefaultPrivilegeIds)

  # For debug
  def _dumpClassMapById(self):
      header = "=" * 80
      print(header)
      print("Class map by id (%d ids)" % ( len(self._classIdMap)))
      print(header)

      mmax = len(max(self._classIdMap, key=len))

      formatString = "%%-%d.%ds %%6d" % ( mmax, mmax )
      for id in sorted( self._classIdMap.keys()):
          print(formatString % ( id, self._classIdMap[id] ))
      print ("")

  # For debug
  def _dumpClassMapByValue(self):
      revmap = dict((v,k) for k,v in list(self._classIdMap.items()))

      header = "=" * 80
      print (header)
      print ("Class map by value (%d values)" % ( len(revmap)))
      print (header)

      for value in sorted( revmap.keys()):
          print ("%-6d %s" % ( value, revmap[value] ))
      print ("")

  def extractField(self, data, field):
      self.metadata_current_field = field
      if field in self._metadataHandlers:
          func = self._metadataHandlers[field][1]
          # Fetch the data for the given field if it exists, otherwise use the
          # default.
          if field in data:
              value = data[field]
          else:
              value = self._metadataHandlers[field][2]

          # If we have a value, process it now.  If no value exists, such as the
          # case where there is no default, then return the value which is None
          if value != None:
              s = func(value)
          else:
              s = value
          return s
      else:
          for valid_name in self._metadataHandlers:
              if valid_name.lower()[:3] == field.lower()[:3]:
                  raise Exception('Invalid metadata field name "%s" - should be "%s"' %
                                   (field, valid_name))

          valid_names = ', '.join(sorted(self._metadataHandlers))
          raise Exception('Invalid metadata field name "%s" - should be one of: %s' %
                          (field, valid_names))

  def generate_metadata(self, data):
      """
      Turns a set of program attributes in human readable form into a
      string parsed by the mink subsystem.

      Args:

          Note: following are supported fields in QTVM by now:
            appName, privileges, services

          There is one required argument, md (metadata).  The md argument
          contains program attributes in a human readable form.  md is a
          dictionary that contains the following fields:

          md = {
            'privileges':  <iterable of string ids>,
            'services':    <iterable of string ids>,
            'appName':     <string containing 1-31 characters>
          }

          Description of each of the fields:

              privileges:  Iterable of strings corresponding to class IDs or
                           services IDs.

                           The ID fields are are the same as the corresponding
                           define for the class ID, minus the leading 'C' and
                           trailing '_UID'.  The ID for the CI2C_UID is 'I2C';
                           the ID for the CDeviceID_UID is 'DeviceID', etc.

              services:    Iterable of strings corresponding to class IDs of services.
                           It's the set of IDs of services that the app exports.
                           Other apps requesting access to the service will need
                           to add the same ID to its list of privileges.

              appName:     A string of 1 to 31 characters.

          None of the fields is required.  If a field is present, the name
          must be exactly as above, including the case.

          The IDs used in the privileges and services fields are case independent -
          'DEVICEID', 'deviceid', and 'DeviceId' are all the same ID.

      Examples:

          md = {
              'appName':     'TestApp',
              'privileges':  ['MetadataTestPrivilege'],
              'services':    ['MetadataTestService']
          }

      Raises:

          The base level Exception class is the only exception raised by this function.

          This function tries to do as much validation as possible.  Some conditions
          that will raise an Exception:

              1) If the input metadata has an attribute that is not recognized.
              2) If a field has a value that is not of the correct type.
              2) If a field has a value that is not in range.
              3) If any ID in the privileges or services field is not recognized.
              4) Other conditions.

      """

      metadata = {}

      if (not 'privileges' in data):
          data = copy.copy(data)
          if not 'privileges' in data:
              data['privileges'] = self._metadataHandlers['privileges'][2]

      # Process the metadata.
      for field in data:

          if field in self._metadataHandlers:
              abbr = self._metadataHandlers[field][0]
              s = self.extractField(data, field)
              if s is None:
                  continue
              if abbr in metadata:
                  raise Exception('Field "%s" appears multiple times.' % ( field ))
              metadata[abbr] = s
          else:
              for valid_name in self._metadataHandlers:
                  if valid_name.lower()[:3] == field.lower()[:3]:
                      raise Exception('Invalid metadata field name "%s" - should be "%s"' %
                                       (field, valid_name))

              valid_names = ', '.join(sorted(self._metadataHandlers))
              raise Exception('Invalid metadata field name "%s" - should be one of: %s' %
                              (field, valid_names))

      # Concatenate the result for each metadata field into one big string.

      output_values = ['%s=%s' % (abbr, metadata[abbr]) for abbr in sorted(metadata.keys())]

      str = ';'.join(output_values)

      return str

  def check_class_ids(self):
    '''
    verifies that all class IDs are within Qualcomm range
    Qualcomm: 0 - 0xffffff
    OEM:      0x1000000 - 0x1ffffff

    This should only be invoked in internal builds, as external builds
    may have .idl files written by oems that contain class IDs in their
    range
    '''
    for k,v in list(self._classIdMap.items()):
      if not 0 <= v < 0x1000000:
        raise Exception('Class ID {} with value {} is outside of the Qualcomm range \
        Please give it a new class id within 0 - 0xffffff'.format(k, hex(v)))

  def _gen_class_set(self, idset):
      """
      Returns a set of class numbers
      """

      values = set()
      for id in idset:
          #  Identifier - alphanumeric beginning with a non-digit.
          classId = self._check_str(r'[a-zA-Z_][a-zA-Z_0-9]*', id)
          key = classId.lower()

          if key == 'default':
              values.update(self._defaultPrivileges)
          elif key in self._classIdMap:
              classNum = self._classIdMap[key]
              values.add(classNum)
          else:
              raise Exception('Invalid metadata class id: %s' % (id))

      return values

  def _gen_class_set_string(self, idset):
      values = self._gen_class_set(idset)

      if len(values) == 0:
          return None

      return _encode_id_set(values)

  def _check_boolean(self, str):
    if (str == 'true') or (str == 'false'):
        return self._escape(str)
    else:
        raise Exception('Metadata %s value "%s" does not equal to "true" or "false" ' %
            (self.metadata_current_field, str))

  def _check_str(self, regex, str):
      if re.match('^(' + regex + ')$', str):
          return self._escape(str)
      else:
          raise Exception('Metadata %s value "%s" does not conform to pattern /%s/' %
              (self.metadata_current_field, str, regex))

  def _escape(self, value):
      return value.replace('%', '%25'). \
                   replace(',', '%2C'). \
                   replace(';', '%3B'). \
                   replace('=', '%3D')

#-------------------------------------------------------------------------------
#  Show Manual.
#  Examples:
#    python SecureAppMetadata.py -h               # Show Manual.
#    python SecureAppMetadata.py -i <input_file>  # Convert it and
#                                                 #   then do some auto-generation.
#                                                 #   The input file stores metadata.
#                                                 #   By default, the auto-generated
#                                                 #   file would be located at same
#                                                 #   path of input file.
#    python SecureAppMetadata.py -i ../testapp/src/Metadata.json
#-------------------------------------------------------------------------------
def help():
    print('Help:\n\t-h: Show Manual')
    print('\t--i <input_file>: Convert it and then do some auto-generation')
    print('\t--p <project_root>:')

#-------------------------------------------------------------------------------
#  Convert metadata into a string.
#-------------------------------------------------------------------------------
def metadata2str(input_md, proj_root):
    # Assuming that this Python script is located at ./sdk/
    # If you changed the location of this Python script,
    #   then you might have to change the path below
    #   which is the uppermost directory to be scanned.
    sdk_root = os.path.realpath(os.path.join(os.path.dirname(sys.argv[0]), "./"))
    idl_class_ids_sdk = IDLScanner()._generate_class_id_map(sdk_root)
    idl_class_ids_proj = IDLScanner()._generate_class_id_map(proj_root)
    print("SDK provided IDLs: ")
    print(idl_class_ids_sdk)
    print("Project provided IDLs: ")
    print(idl_class_ids_proj)

    if (len(idl_class_ids_sdk)+len(idl_class_ids_proj)) == 0:
        raise("No IDL headers present")

    idl_class_ids_sdk.update(idl_class_ids_proj)
    md_class = SecureAppMetadataClass(idl_class_ids_sdk)
    return md_class.generate_metadata(input_md)

#-------------------------------------------------------------------------------
#  Generate metadata.c automatically.
#  By default, metadata.c would be located at same path of input_metadata_file.
#-------------------------------------------------------------------------------

def encode_c_str(str):
    str = re.sub(r'[?"\\]', lambda m: '\\' + m.group(0), str)
    str = re.sub(r'[\x00-\x1f]', lambda m: '\\%03o' % ord(m.group(0)), str)
    return '"' + str + '"'

# Instead of assembling the metadata file line by line, we now write it out here, and
# use python string formatting, combined with keyword arguments, to fill in the gaps
# according to the metadata that was passed to SecureAppBuilder.
# See https://pyformat.info/ for proper usage of python's string format()
# Some things to keep in mind:
#   - if adding new metadata fields, be certain that the variable name you want to replace
#     doesn't already exist in the format string
#   - be very sure if you're trying to format in a raw string (like we are doing for uuid_string)
#     or otherwise (like abuf)
#   - if you want an actual brace to appear in the output string, you need to double up the braces
ENTIRE_FILE_FORMAT_STRING = '''
//-------------------------------------------------------------------------
// Auto-generated file.  Do not edit.
//-------------------------------------------------------------------------
#include <stddef.h>
#include <stdbool.h>

#define TA_SYMBOL_EXPORT __attribute__((visibility("default")))

TA_SYMBOL_EXPORT const char __attribute__((section(".metadata.sentinel"))) TA_METADATA[] = {ta_metadata_str};

'''

def auto_generate_c_file(str, out_path):

    final_str = encode_c_str(str)
    whole_file = ENTIRE_FILE_FORMAT_STRING.format(
       ta_metadata_str  = final_str,
       )

    # Read the contents of the existing metadata.segment file
    old_data = ""
    segment_data = str + '\0'
    if os.path.exists(out_path):
      with open(out_path, 'r') as o:
        old_data = o.read()

    # Create new metadata.segment file if new metadata contents don't match with
    # existing data.
    if segment_data != old_data:
      with open(out_path, 'w') as output:
        print("Generating module metadata segment file" + out_path)
        output.write(segment_data)
    else:
        print("Module metadata segment file " + out_path + " already_exists")

#-------------------------------------------------------------------------------
#  Read and parse json file - Metadata.json and then
#    return a dict valuable - md.
#  Example:
#    input json file looks like:
#    {
#       "appName": "TestApp",
#       "privileges": ["MetadataTestPrivilege"],
#       "services": ["MetadataTestService"]
#    }
#    output dict valualbe - md:
#      type of md: <type 'dict'>
#      md = {
#        'appName':     'TestApp',
#        'privileges':  ['MetadataTestPrivilege'],
#        'services':    ['MetadataTestService']
#      }
#-------------------------------------------------------------------------------

# By default, the data read from json file would be of Unicode format like below:
#   {u'services': [u'MetadataTestService'], u'privileges': [u'MetadataTestPrivilege'], u'appName': u'TestApp'}
#   where each item is not actual string. So we should remove those Unicode symbols and
#   converted dict can be parsed successfully later.
# lazy flag is used to reduce redundant recursion to remove Unicode
#   in following occasions:
#   a) 'large' json object
#   b) much data copies when invoking functions
# We use it by object_hook mechanism of json library. For more details,
# please reference: https://docs.python.org/2/library/json.html#json.load
def remove_unicode(data, lazy = False):
    if isinstance(data, str):
        return data

    if isinstance(data, list):
        return [ remove_unicode(item, lazy = True) for item in data ]

    if isinstance(data, dict) and not lazy:
        return {
            remove_unicode(key, lazy = True): remove_unicode(value, lazy = True)
            for key, value in data.items()
        }

    # Convert Unicode format into real string
    if str(type(data)) == "<type 'unicode'>":
        return data.encode('utf-8')

    return data

# We will check bad fields in later generate_metadata()
def read_metadata_json(input_file):
    with open(input_file, 'r') as load_f:
        return remove_unicode(
            json.load(load_f, object_hook=remove_unicode),
            lazy = True
        )

#-------------------------------------------------------------------------------
#  Main Function.
#-------------------------------------------------------------------------------

if __name__ == '__main__':
    input_file = ''
    argv = sys.argv[1:]
    proj_root = ''
    out_path = ''

    try:
        opts, args = getopt.getopt(argv, "h:", ["i=", "p="])
    except getopt.GetoptError:
        print('Incorrect cmd sent')
        help()
        sys.exit(2)

    for opt, arg in opts:
        if opt == '-h':
            help()
            sys.exit()
        elif opt in ("--i"):
            input_file = arg
            md = read_metadata_json(input_file)
            input_file_base_name = os.path.basename(input_file)
            pos = input_file_base_name.rfind('.')
            if pos == -1:
                output_file_base_name = input_file_base_name + ".txt"
            else:
                output_file_base_name = input_file_base_name[0 : pos] + ".txt"
            out_path = os.path.realpath(os.path.dirname(input_file)) + "/" + output_file_base_name
        elif opt in ("--p"):
            proj_root = os.path.realpath(arg)

    auto_generate_c_file(metadata2str(md, proj_root), out_path)
    sys.exit()
