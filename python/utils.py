#!/usr/bin/env python
#-*- coding:utf-8 -*-
#author:lijian
#date: 2016
#Copyright: free

import os
import commands
import ConfigParser
import time
import subprocess
import sys
import socket
import logging

def execShellCommand(cmd, wait=0, fast=False, verbose=False):
    """
    execute a linux shell command
    param cmd: a string, shell command
    param wait: timeout for shell execute
    """
    p = subprocess.Popen(cmd, shell=True,
                         stdout=None if verbose else subprocess.PIPE,
                         stderr=None if verbose else subprocess.PIPE)
    out, err = p.communicate()
    time.sleep(wait)
    if p.returncode != 0:
        if fast:
            sys.exit(1)
    
    return out, err, p.returncode

def uuid():
    out,err,errcode = execShellCommand("uuidgen")
    if out.endswith("\n"):
        out = out[:-1]
    return out

def mergeMultiSpace(s):
    while True:
        s1 = s.replace('  ', ' ')
        if s == s1:
            return s
        s = s1

def checkPort(ip, port):
    """
    check the host ip port is started or not
    """
    try:
        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.connect((ip,int(port)))
        s.close()
        return True
    except:
        return False

def setConfigField(filePath, field, key, value):
    """
    set cfg file field, like this:

    [field]
    key = value

    param filePath: cfg file path
    """
    try:
        fd = None
        if filePath is None or not os.path.exists(filePath):
            return False
        config = ConfigParser.ConfigParser()
        fd = open(filePath, 'r')
        config.readfp(fd)
        fd.close()
        config.set(field, key, value)
        fd = open(filePath, 'w')
        config.write(fd)
        fd.close()
    except:
        return False

    return True

def getConfigField(filePath, field, key):
    """
    get cfg file field-key's value, like this:

    [field]
    key = value

    param filePath: cfg file path
    """

    value = '' 
    try:
        fd = None
        if filePath is None or not os.path.exists(filePath):
            return value
        config = ConfigParser.ConfigParser()
        fd = open(filePath)
        config.readfp(fd)
        value = config.get(field, key)
        fd.close()
    except:
        pass
        
    return value

def timestamp():
    """
    get timestamp
    """
    return int(time.time())

def ismount(mount):
    """
    check this dev is mounted
    """
    cmd = "lsblk -l | grep %s| wc -l" %(mount)
    status,out = commands.getstatusoutput(cmd)
    if(int(out) > 0):
        return True
    else:
        return False

def mountdev(dev, mount_dir):
    """
    mount dev to a directory
    """
    cmd = "mount -t ext4 %s %s" %(dev, mount_dir)
    commands.getstatusoutput(cmd)

def umountdev(dev):
    """
    umount device
    """
    cmd = "umount -lf %s" %(dev)
    commands.getstatusoutput(cmd)

def getHostname():
    """
    get hostname
    """
    status,out = commands.getstatusoutput('hostname')
    return out

def touchFile(filePath):
    """
    If a file at filePath already exists, its accessed and modified times are
    updated to the current time. Otherwise, the file is created.
    :param filePath: The file to touch
    """
    with open(filePath, 'a'):
        os.utime(filePath, None)

def _parseMemInfo(lines):
    """
    Parse the content of ``/proc/meminfo`` as list of strings
    and return its content as a dictionary.
    """
    meminfo = {}
    for line in lines:
        var, val = line.split()[0:2]
        meminfo[var[:-1]] = int(val)
    return meminfo


def readMemInfo():
    """
    Parse ``/proc/meminfo`` and return its content as a dictionary.

    For a reason unknown to me, ``/proc/meminfo`` is sometimes
    empty when opened. If that happens, the function retries to open it
    3 times.

    :returns: a dictionary representation of ``/proc/meminfo``
    """
    # FIXME the root cause for these retries should be found and fixed
    tries = 3
    while True:
        tries -= 1
        try:
            with open('/proc/meminfo') as f:
                lines = f.readlines()
                return _parseMemInfo(lines)
        except:
            logging.warning(lines, exc_info=True)
            if tries <= 0:
                raise
            time.sleep(0.1)

def tobool(s):
    """
    a value to bool
    """
    try:
        if s is None:
            return False
        if type(s) == bool:
            return s
        if s.lower() == 'true':
            return True
        return bool(int(s))
    except:
        return False