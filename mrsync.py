#!/usr/bin/env python

import os,sys,string,time,getopt;

my_module_path = os.path.dirname(sys.argv[0]);
sys.path.append(my_module_path); 
from mrsync_config import *

host = os.uname()[1];

def bin(path, bin_name):
    "return the full-path of a binary code"
    return '%s/%s' % (path, bin_name);

def isPathThere(path):
    "check the existence of path on this master machine"
    if not os.path.exists(path):
        print >>sys.stderr, 'Path (%s) could not be found on %s' % (path, host);
        sys.exit(-1);

map(isPathThere, [bin(binDir, x) for x in codes]);

def printTimeMsg(msg):
    print >>sys.stderr, 'Time = %s %s' % (time.ctime(time.time()), msg);

def prMulticastLog(msg):
    "append msg to the log file"
    open(multicast_log, 'a').write('%s %s\n' % (time.ctime(time.time()), msg));
 
(catcher_err_log, goodTargetsFile, syncFileList) = \
                  map(lambda x:
                      '%s.%s' % (x, ('%02d'*5) % time.localtime()[1:6]), 
                      (catcher_err_log, goodTargetsFile, syncFileList));

def usage():
    print """mrsync.py (to sync files from one to many machines by multicast)
    Option list:
    [ -v <verbose_level 0-2 (2 is for debug)> ]
    [ -w <ack_wait_times default= 60 (secs) ]
    [ -r <remote shell to invoke multicatcher, default=rsh> ]
    [ -b <remote_bin_dir_path for multicatcher, default=%s> ]
    [ -o <more rsync options such as --include --exclude,
          to be appended to default min_opts = %s>]
    [ -x flag to turn off monitor mechanism (not fully tested and not recommended) ]
    ----- Essential options --------------------------------------------------------    
      -m <machine_list_file_path or csv_list (name1,name2...)>
      -s <source_data_path>
    [ -t <target_data_path; default = that in -s option> ]
    [ -l <list of files(wildcards) to be synced, can be a filepath or csv_list>
          mrsync by default uses rsync to find the list unless this option is given. ]
    ----- mcast options ------------------------------------------------------------
    [ -A <my_MCAST_ADDR such as 239.255.67.92> ]
    [ -P <my_PORT such as 9000> ]
    [ -T <my_TTL default=1> ]
    [ -L flag to turn on mcast_LOOP. default is off ]
    [ -I <my_MCAST_IF default=NULL> ]
    """ % (rBinDir, min_rsync_opt%reshell);

# --- handle command line
opts, args = getopt.getopt(sys.argv[1:], 'hxv:w:r:b:o:m:s:t:l:A:P:T:LI:', []);

if len(opts)==0 or len(args)>0:
    usage();
    sys.exit(-1);
    
verbose         = 0;
ack_wait_times  = -1;
without_monitor = False;    
machineListFile = '';
sourcePath      = '';
targetPath      = '';
synclist        = '';
rsync_opts      = min_rsync_opt % reshell;

mcast_addr      = '';
port            = -1;
ttl             = 1;
loop            = False;
mcast_if        = '';
   
if not len(opts) == 0:
    for o,v in opts:
        if o=='-v':
            verbose = string.atoi(v);
        elif o=='-w':
            ack_wait_times  = string.atoi(v);
        elif o=='-h':
            usage();
            sys.exit(0);
        elif o=='-r':
            reshell = v;
        elif o=='-b':
            rBinDir = v;
        elif o=='-o':
            rsync_opts += (' %s' % v)
        elif o=='-m':
            machineListFile = v;
        elif o=='-s':
            sourcePath = v;
        elif o=='-t':
            targetPath = v;
        elif o=='-l':
            synclist = v;
        elif o=='-A':
            mcast_addr = v;
        elif o=='-P':
            port = string.atoi(v);
        elif o=='-T':
            ttl = string.atoi(v);
        elif o=='-L':
            loop = True;
        elif o=='-I':
            mcast_if = v;
        elif o=='-x':
            without_monitor = True;

if verbose>=1: print 'mrsync version 4.0.0';

if not machineListFile or not sourcePath:
    print >>sys.stderr, 'Essential options (-m -s) should be specified.';
    usage();
    sys.exit(-1);
    
isPathThere(sourcePath);
if not targetPath: targetPath = sourcePath;

# ------------ get machine list
machines = (os.path.exists(machineListFile) and
            [x[:-1] for x in open(machineListFile).readlines()] or
            machineListFile.split(','));

# clean up the names
machines = filter(lambda x: x!='', machines);
machines = [x.strip() for x in machines];

if host in machines:
    if verbose>=1: print 'remove myself (%s) from machine list...' % host;
    machines.remove(host);

# ------------ get the syncList from the first good machine
#              the list is stored in syncFileList for multicaster.
import cmdToTarget;

def cleanup(file):
    "remove a file if it exists"
    if os.path.exists(file): os.unlink(file);

def get_synclist_from_cmdline(tmp):
    "extracts synclist from cmdline option, outputs results into tmp_file"
    cleanup(tmp);
    if verbose>=1: print  >>sys.stderr, 'extracting %s...' % synclist;

    list = (os.path.exists(synclist) and
            [x[:-1] for x in open(synclist).readlines()] or
            synclist.split(','));
    
    if len(list)==0:
        print >>sys.stderr, "Empty sync_list in cmd_line option %s" % synclist;
        sys.exit(-1);

    def pr_relative_path(fullpath):
        open(tmp, 'a').write('%s\n' % fullpath.replace(sourcePath+'/', ''));
        
    for item in list:
        " each item can be a pattern for files"
        import glob
        map(pr_relative_path, glob.glob('%s/%s' % (sourcePath, item)));

            
def get_synclist_from_rsync(tmp):
    "use rsync to get a list of to-be-synced files. results are put in tmp file"
    cleanup(tmp);
    for machine in machines:
        # check if this machine is ok to rsh (ssh)
        if (not cmdToTarget.isMachineOK(reshell, machine)):   # if not go to next machine
            continue;

        if verbose>=1:
            print  >>sys.stderr, 'Get to-be-synced files from %s...' % machine;

        cmd = ' '.join(filter(lambda x: x,
                              [rsyncPath, '--rsync-path='+remote_rsyncPath, \
                               (reshell != 'rsh' and \
                                rsync_opts.replace('--rsh=rsh', '--rsh=%s' % reshell) \
                                or rsync_opts),
                               sourcePath+'/',
                               '%s:%s/' % (machine, targetPath),
                               '> %s 2>&1; ' % tmp]));
        
        if verbose>=1: print >>sys.stderr, cmd;
        os.system(cmd);
        break;

def tmp_synclist():
    "intermediate synclist filename"
    return  '/tmp/%s' % os.path.basename(syncFileList);

(synclist and get_synclist_from_cmdline or get_synclist_from_rsync)(tmp_synclist());

def ckFileSize(file):
    if os.path.getsize(file)==0:
        print >>sys.stderr, "Empty file = %s" % file;
        sys.exit(-1);

ckFileSize(tmp_synclist());

# translate the files generated by rsync or command-line option into
# a format which can be recognized by multicaster.
cmd = ' '.join([bin(binDir, translate),
                tmp_synclist(), sourcePath, '>', syncFileList]);
if verbose>=1: print >>sys.stderr, cmd;
os.system(cmd);

ckFileSize(syncFileList);

# ------------ invoke multicatcher on all target machines
def gen_catcher_cmd(count):
    "return mulitcatcher_command to be invoked on target machines"
    return ' '.join(filter(lambda x: x,
                           [bin(rBinDir, catcher),
                            '-t', targetPath,
                            '-i', '%d'%count, # machine id
                            (mcast_addr and '-A '+mcast_addr or ''),
                            (port>0 and '-P %d'%port or ''),
                            (mcast_if and '-I %s'%mcast_if or ''),
                            '</dev/null 1>/dev/null 2>%s &' % catcher_err_log])); ### workaround ssh problem
    
def invoke_catcher(ms, count, bads):
    "invoke catcher for each machine in ms, return bad_machines in bads"
    if not ms: return bads;
    
    machine = ms.pop(0);

    if (not cmdToTarget.isMachineOK(reshell, machine)):
        if verbose>=1: print >>sys.stderr, "***%s is down" % machine;
        bads.append(machine);
        return invoke_catcher(ms, count, bads);

    cmd = gen_catcher_cmd(count);
    if count==0 and verbose>=1: print >>sys.stderr,cmd;
    status, output = cmdToTarget.docmd(reshell, machine, cmd); 
    
    if (not status):
        if verbose>=1: print >>sys.stderr, "***remote shell exec failed for %s" % machine;
        bads.append(machine);
        return invoke_catcher(ms, count, bads);

    if verbose>=1: print >>sys.stderr, 'id:%3d %s' % (count, machine);
    open(goodTargetsFile, 'a').write('%s\n' % machine)
    return invoke_catcher(ms, count+1, bads);

printTimeMsg("Invoking multicatcher on all %d machines..." % len(machines));
cleanup(goodTargetsFile);
badMachines = invoke_catcher(machines, 0, []);

# -------------- invoke multicast on the master machine
printTimeMsg('Starting multicasting...');
prMulticastLog('start multicast on %s' % host);

def gen_caster_cmd():
    "return mulitcaster_command to be invoked on this host (master machine)"
    return ' '.join(filter(lambda x: x,
                           [bin(binDir, caster),
                            '-v %d' % verbose,
                            '-m %s' % goodTargetsFile,
                            '-s %s' % sourcePath,
                            '-f %s' % syncFileList,
                            (ack_wait_times>0 and '-w %d'% ack_wait_times or ''),
                            (mcast_addr and '-A '+mcast_addr or ''),
                            (port>0 and '-P %d'%port or ''),
                            (ttl>1 and '-T %d'%ttl or ''),
                            (loop and '-L' or ''),
                            (mcast_if and '-I %s'%mcast_if or ''),
                            (without_monitor and '-x' or '')]));

cmd = gen_caster_cmd();
if verbose>=1: print cmd;
ex_code = os.system(cmd);
print >>sys.stderr, 'ex_code= ', ex_code;

# -------------- to exit
printTimeMsg('ALL DONE.');
prMulticastLog('multicast ends on %s' % host);
sys.exit(ex_code);
