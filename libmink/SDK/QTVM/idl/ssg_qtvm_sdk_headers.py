#!/usr/bin/env python
####################################################
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
####################################################

# Note:
#   This Python script file has been tested with
#     Python v3.8.16.

import argparse
import os
import subprocess
import sys

CONSTANT_HEADER = ".h"
CONSTANT_CPP_HEADER = ".hpp"
CONSTANT_SKEL_HEADER = "_invoke.h"
CONSTANT_CPP_SKEL_HEADER = "_invoke.hpp"

def get_intermediates_path(module_name, package):
    """Derive the soong intermediates gen directory at runtime.

    In an sbox genrule sandbox TMPDIR is set to a path of the form:
        <build_root>/soong/.temp/sbox/<hash>/tmp
    We locate the '/soong/' segment to recover the soong output base, then
    construct the standard intermediates path without any hardcoded prefix.

    Falls back to OUT_DIR if TMPDIR does not contain '/soong/'.
    """
    tmpdir = os.environ.get('TMPDIR', '')
    soong_marker = '/soong/'
    idx = tmpdir.find(soong_marker)
    if idx >= 0:
        soong_base = tmpdir[:idx + len(soong_marker) - 1]  # up to and including '/soong'
    else:
        out_dir = os.environ.get('OUT_DIR', 'out')
        soong_base = os.path.join(out_dir, 'soong')

    return os.path.join(soong_base, '.intermediates', package, module_name, 'gen')

def gen_cmd_headers(minkidlpath, output_path, input, cpp, skel):
    if not cpp and not skel:
        output = os.path.join(output_path, os.path.splitext(os.path.basename(input))[0] + CONSTANT_HEADER)
        cmd = [minkidlpath, "-o", output, input]
    elif cpp and not skel:
        output = os.path.join(output_path, os.path.splitext(os.path.basename(input))[0] + CONSTANT_CPP_HEADER)
        cmd = [minkidlpath, "-o", output, input, "--cpp"]
    elif not cpp and skel:
        output = os.path.join(output_path, os.path.splitext(os.path.basename(input))[0] + CONSTANT_SKEL_HEADER)
        cmd = [minkidlpath, "-o", output, input, "--skel"]
    else:  # cpp and skel
        output = os.path.join(output_path, os.path.splitext(os.path.basename(input))[0] + CONSTANT_CPP_SKEL_HEADER)
        cmd = [minkidlpath, "-o", output, input, "--cpp", "--skel"]
    print(f"output file: {output}")
    return cmd

def gen_qtvm_sdk_headers(input, tool, module_name, package, cpp, skel):
    error_count = 0
    out_path = get_intermediates_path(module_name, package)

    print(f"out path (abs): {os.path.abspath(out_path)}")
    try:
        os.makedirs(out_path, exist_ok=True)
    except Exception as e:
        print(f'mkdir failed: {e}')

    for h in input:
        cmd = gen_cmd_headers(tool, out_path, h, cpp, skel)
        result = subprocess.call(cmd)
        if result != 0:
            print('error: gen_qtvm_sdk_headers: cmd %s failed %d' % (cmd, result))
            error_count += 1

    print('gen', "Success" if error_count == 0 else "Fail!")
    return error_count

def main():
    """Parse command line arguments and perform top level control."""
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    # Arguments that apply to every invocation of this script.
    parser.add_argument(
        '--input', type=str, nargs='*', required=True,
        help='Input IDL files.')
    parser.add_argument(
        '--tool', type=str, required=True,
        help='Path to minkidl tool.')
    parser.add_argument(
        '--name', type=str, required=True,
        help='Module name (used to locate the intermediates gen directory).')
    parser.add_argument(
        '--package', type=str, required=True,
        help='Android package path of this module (e.g. vendor/qcom/opensource/…/idl).')
    parser.add_argument(
        '--cpp', action='store_true',
        help='Generate c++ header.')
    parser.add_argument(
        '--skel', action='store_true',
        help='Generate skeleton header (instead of stub header).')

    args = parser.parse_args()

    print('Input files: [%s]' % args.input)
    print('Tool path:   [%s]' % args.tool)
    print('Module name: [%s]' % args.name)
    print('Package:     [%s]' % args.package)
    print('cpp flag:    [%s]' % args.cpp)
    print('skel flag:   [%s]' % args.skel)
    return gen_qtvm_sdk_headers(args.input, args.tool, args.name, args.package, args.cpp, args.skel)

if __name__ == '__main__':
    sys.exit(main())