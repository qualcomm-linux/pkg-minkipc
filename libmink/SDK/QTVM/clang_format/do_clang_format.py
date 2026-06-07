# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear

import os
import argparse
import pathlib

def do_format_single(dirname, basename, style_file):
    fullpath = os.path.join(dirname, basename)
    target_style = os.path.join(dirname, ".clang-format")
    print(fullpath)
    os.system("cp %s %s" % (style_file, target_style))
    os.system("clang-format -style=file -fallback-style=none -i %s" % fullpath)
    os.system("rm %s -f" % target_style)

def do_format_dir(target_path, style_file):
    for root, ds, fs in os.walk(os.path.realpath(target_path)):
        for f in fs:
            if f.endswith('.c') or f.endswith('.h') or f.endswith('.cpp') or f.endswith('.hpp'):
                do_format_single(root, f, style_file)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('-s', '--style-file',
                        type=argparse.FileType('r', encoding='UTF-8'),
                        default=os.path.dirname(__file__) + '/ABC.clang-format',
                        metavar="<style file>",
                        help="clang-format style file to use for formatting" )

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-d', '--dir',
                       type=pathlib.Path,
                       metavar="<directory>",
                       help="Parent directory of files to be formatted" )
    group.add_argument('-f', '--file',
                       type=argparse.FileType('r', encoding='UTF-8'),
                       metavar="<file>",
                       help="File to be formatted" )

    args = parser.parse_args()

    style_file = os.path.realpath(args.style_file.name)

    if args.dir:
        if args.dir.is_dir():
            do_format_dir(args.dir, style_file)
        else:
            print("error: %s is not a directory or does not exist." % args.dir)
    else:
        dirname, basename = os.path.split(os.path.realpath(args.file.name))
        do_format_single(dirname, basename, style_file)
