#!/usr/bin/env python3

"""Saves/restores Mac metadata (specifically resource forks and Finder info)"""

import argparse
import ctypes
import os
import os.path
import pickle
import shlex
import shutil
import struct
import sys

lc = ctypes.CDLL("libc.dylib")

TOOLS_PATH = "/Developer/Tools"

# Make sure our tools path is in the PATH.
tools_env = os.environ.copy()
tools_env["PATH"] += ":" + TOOLS_PATH

MD_DIR = ".mac_md"
ATTR_FILE = ".__mac_attr__"
RSRC_FILE = ".__mac_rsrc__"

RSRC_FORK = "..namedfork/rsrc"

ATTRIB_MAP = {"attributes": "-a",
              "creator": "-c",
              "type": "-t",
              "created": "-d",
              "modified": "-m"}

def metadata_path(filename):
    return os.path.join(MD_DIR, filename)

def attr_filename(filename):
    return os.path.join(metadata_path(filename), ATTR_FILE)

def rsrc_filename(filename):
    return os.path.join(metadata_path(filename), RSRC_FILE)

def rsrc_fork_name(filename):
    return os.path.join(filename, RSRC_FORK)

def list_xattrs(filename):
    list_size = lc.listxattr(filename, 0, 0, 0);
    if list_size:
        list_buf = ctypes.create_string_buffer(list_size);
        lc.listxattr(filename, list_buf, len(list_buf), 0)
        attr_names = list_buf.raw[:-1].split('\0')
    else:
        attr_names = []

    return attr_names
    
def set_xattrs(filename, attrs):
    current_attrs = list_xattrs(filename)

    remove_attrs = filter(lambda x: x not in attrs, current_attrs)

    for name, value in attrs.iteritems():
        lc.setxattr(filename, name, value, len(value), 0, 0)

    for name in remove_attrs:
        lc.removexattr(filename, name, 0)
	
def get_xattrs(filename):
    attr_names = list_xattrs(filename)
    attrs = {}

    for name in attr_names:
        attr_size = lc.getxattr(filename, name, 0, 0, 0, 0)
        attr_buf = ctypes.create_string_buffer(attr_size)
        lc.getxattr(filename, name, attr_buf, len(attr_buf), 0, 0)

        attrs[name] = attr_buf.raw

    return attrs

    
def parse_args(argv=None):
    p = argparse.ArgumentParser(description=__doc__)

    p.add_argument("files", nargs="*", help="Files to process", metavar="file")
    p.add_argument("-r", "--restore", action='store_const', dest="operation",
                   default=save_metadata, const=restore_metadata,
                   help="Restore metadata instead of saving")
    p.add_argument("-R", "--recursive", action="store_true",
                   help="Drill down into directories")
    p.add_argument("-0", "--nulls", action="store_true",
                   help="Print nulls (for xargs) after filenames.")
    p.add_argument("-g", "--git", action="store_true",
                   help="Parses input as git --porcelain lines, not files.")
    p.add_argument("-p", "--print", dest="print_names", action="store_true",
                   help="Print generated file names (for adding to repo)")
    p.add_argument("-d", "--debug", dest="debug", action="store_true",
                   help="Print REALLY verbose debug information")
    p.add_argument("-n", "--dryrun", dest="dry", action="store_true",
                   help="Do not write any files.")
    p.add_argument("-v", "--verbose", action="store_true",
                   help="Print out additional information")
                    
                    
    return p.parse_args(argv)

def print_filename(filename, args):
    if args.nulls:
        sys.stdout.write(filename + '\0')
    else:
        print("'%s'" % filename)

def restore_metadata(filename, args):
    if not os.path.exists(filename):
        raise OSError("Trying to restore metadata to a nonexistent file")

    afile = attr_filename(filename)

    # Do the attributes second, since we might affect the modification date.
    if os.path.exists(afile):
        attrs = pickle.load(open(afile, "rb"))
    if not args.dry:
            set_xattrs(filename, attrs)

def save_metadata(filename, args):
    if not os.path.exists(filename):
        raise OSError("Trying to save metadata for a nonexistent file")

    is_dir = os.path.isdir(filename)
    
    #attrs = get_attributes(filename, is_dir, args)
    attrs = get_xattrs(filename)
    #rsrc = None if is_dir else get_rsrc(filename)

    md_path = metadata_path(filename)

    if not os.path.exists(md_path) and not args.dry:
        os.makedirs(md_path)

    # If we have attributes, pickle them out.  The xattr method saves the rsrc
    # fork as well as attribute metadata; no more second step.
    if attrs:
        afile = attr_filename(filename)
        try:
            if not args.dry:
                f = open(afile, "wb")
                pickle.dump(attrs, f)
        except IOError as e:
            raise IOError("Error saving %s: %s" % (afile, str(e)))

        if args.print_names: print_filename(afile, args)

def delete_metadata(filename, args):
    afile = attr_filename(filename)

    if os.path.exists(afile) and not args.dry:
        unlink(afile)

def is_in_metadata_dir(pathname):
    return MD_DIR == pathname[:len(MD_DIR)]

def move_metadata(files, args, copy=False):
    to_file, from_file = files
    #print >> sys.stderr, "Rename to:", to_file, "from:", from_file

    ffile = attr_filename(from_file)
    tfile = attr_filename(to_file)

    tdir = os.path.dirname(tfile)
    fdir = os.path.dirname(ffile)

    if not args.dry:
        if not os.path.exists(tdir): os.makedirs(tdir)

        try:
            if os.path.exists(ffile):
                if not copy: shutil.move(ffile, tfile)
                else:        shutil.copy2(ffile, tfile)

                if args.print_names:
                    print_filename(tfile, args)
                    if not copy:
                        print_filename(ffile, args)
        except IOError as e:
            # Probably because we already did it?
            #print >> sys.stderr, "Unable to move %s->%s: %s" % (ffile, tfile, str(e))
            pass


def copy_metadata(files, args):
    return move_metadata(files, args, copy=True)

def add_file_to_process(files, pathname, next_pathname, args):
    used_next = False

    if args.git:
        # Our field spacing differs depending on the operation because git-diff
        # has different spacing from git-status.
        if args.operation == save_metadata:
            type, file = pathname[:3], pathname[3:]
            operations = {"A": (save_metadata, False),
                          "M": (save_metadata, False),
                          "D": (delete_metadata, False),
                          "R": (move_metadata, True),
                          "C": (copy_metadata, True)}
        else:
            # This could possibly blow up on rename ops, but so far they seem
            # to just come out as add/deletes.
            type, file = pathname, next_pathname

            # This doesn't conform to the previous expectations, since the
            # incoming first argument is actually the status.  Fix it up a bit.
            pathname = file
            next_pathname = None

            operations = {"A": (save_metadata, True),
                          "M": (save_metadata, True),
                          "D": (None, True)}
        
        # If there's nothing in the left-hand column, it's not in the index.
        if type[0] == ' ': return False

        if len(pathname) == 0:
            # Just skip it, we got some bonus room.
            return False

        if args.debug: print >> sys.stderr, "op/file:", type, file

        try:
            operation, used_next = operations[type[0]]
        except KeyError:
            print >> sys.stderr, "Weird name:", pathname
            return False

        # We aren't actually storing two paths when we're restoring.
        if used_next and args.operation != restore_metadata:
            file_arg = file, next_pathname
        else:
            file_arg = file
    else:
        file_arg = file = pathname
        operation = args.operation

    # We don't want to try to add metadata for the metadata.
    if is_in_metadata_dir(file): return used_next

    files.add((file_arg, operation))

    if args.recursive and os.path.isdir(pathname):
        descendents = os.walk(pathname)
        #print "descendents:", list(descendents)
        for dir, dirs, filenames in descendents:
            for f in filenames + dirs:
                file = os.path.join(dir, f)
                #print file
                files.add((file, operation))

    # Go up the parents and save them.
    parents = os.path.split(file)[0]
    while parents:
        # Was it already in there? Its parents are, too.  Just stop.
        if (parents, save_metadata) in files:
            parents = ""
        else:
            # We never do anything except update info on parent metadata.
            files.add((parents, save_metadata))
            
            parents = os.path.split(parents)[0]

    return used_next
    
def main():
    to_save = set()
    
    args = parse_args()

    if args.debug:
        print >> sys.stderr, args.files

    if args.git:
        # If it's Git output, don't use any files supplied as arguments, just
        # the ones coming in over stdin (since argument files generally don't
        # have the status information attached).  Make sure to remove the last
        # one, which should be empty because of the terminating NULL.
        args.files = sys.stdin.read().split('\0')[:-1]

    oper = args.operation

    used_next = False
    for i, arg in enumerate(args.files):
	#print >> sys.stderr, arg, used_next
        # If we used the "next" file from the last run, skip this one.
        if not used_next:
            next_file = None if (i+1) >= len(args.files) else args.files[i+1]
            used_next = add_file_to_process(to_save, arg, next_file, args)
        else:
            used_next = False
        
    to_save = sorted(list(to_save))

    for f, oper in to_save:
        if args.verbose:
            print(f)

        try:
            if oper is not None:
                oper(f, args)
        except OSError as e:
            print >> sys.stderr, "Error processing metadata for %s: %s" % (f, str(e))


if __name__ == "__main__":
    main()

