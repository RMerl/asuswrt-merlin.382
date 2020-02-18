from __future__ import print_function

import errno
import logging
import os
import random
import socket
import subprocess
import sys
import time

LOG_TIMEOUT = 60.0
LOG_WAIT = 0.1

def fail(msg):
    logging.error('FAIL')
    sys.exit(msg)

def try_connecting_to_socksport():
    socks_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    if socks_socket.connect_ex(('127.0.0.1', socks_port)):
        tor_process.terminate()
        fail('Cannot connect to SOCKSPort')
    socks_socket.close()

def wait_for_log(s):
    cutoff = time.time() + LOG_TIMEOUT
    while time.time() < cutoff:
        l = tor_process.stdout.readline()
        l = l.decode('utf8')
        if s in l:
            logging.info('Tor logged: "{}"'.format(l.strip()))
            return
        logging.info('Tor logged: "{}", waiting for "{}"'.format(l.strip(), s))
        # readline() returns a blank string when there is no output
        # avoid busy-waiting
        if len(s) == 0:
            time.sleep(LOG_WAIT)
    fail('Could not find "{}" in logs after {} seconds'.format(s, LOG_TIMEOUT))

def pick_random_port():
    port = 0
    random.seed()

    for i in range(8):
        port = random.randint(10000, 60000)
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if s.connect_ex(('127.0.0.1', port)) == 0:
            s.close()
        else:
            break

    if port == 0:
        fail('Could not find a random free port between 10000 and 60000')

    return port

logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)s.%(msecs)03d %(message)s',
                    datefmt='%Y-%m-%d %H:%M:%S')

if sys.hexversion < 0x02070000:
    fail("ERROR: unsupported Python version (should be >= 2.7)")

if sys.hexversion > 0x03000000 and sys.hexversion < 0x03010000:
    fail("ERROR: unsupported Python3 version (should be >= 3.1)")

control_port = pick_random_port()
socks_port = pick_random_port()

assert control_port != 0
assert socks_port != 0

if len(sys.argv) < 3:
     fail('Usage: %s <path-to-tor> <data-dir>' % sys.argv[0])

if not os.path.exists(sys.argv[1]):
    fail('ERROR: cannot find tor at %s' % sys.argv[1])
if not os.path.exists(sys.argv[2]):
    fail('ERROR: cannot find datadir at %s' % sys.argv[2])

tor_path = sys.argv[1]
data_dir = sys.argv[2]

tor_process = subprocess.Popen([tor_path,
                               '-DataDirectory', data_dir,
                               '-ControlPort', '127.0.0.1:{}'.format(control_port),
                               '-SOCKSPort', '127.0.0.1:{}'.format(socks_port),
                               '-Log', 'debug stdout',
                               '-LogTimeGranularity', '1',
                               '-FetchServerDescriptors', '0'],
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)

if tor_process == None:
    fail('ERROR: running tor failed')

wait_for_log('Opened Control listener on')

try_connecting_to_socksport()

control_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
if control_socket.connect_ex(('127.0.0.1', control_port)):
    tor_process.terminate()
    fail('Cannot connect to ControlPort')

control_socket.sendall('AUTHENTICATE \r\n'.encode('utf8'))
control_socket.sendall('SETCONF SOCKSPort=0.0.0.0:{}\r\n'.format(socks_port).encode('utf8'))
wait_for_log('Opened Socks listener')

try_connecting_to_socksport()

control_socket.sendall('SETCONF SOCKSPort=127.0.0.1:{}\r\n'.format(socks_port).encode('utf8'))
wait_for_log('Opened Socks listener')

try_connecting_to_socksport()

control_socket.sendall('SIGNAL HALT\r\n'.encode('utf8'))

wait_for_log('exiting cleanly')
logging.info('OK')

try:
    tor_process.terminate()
except OSError as e:
    if e.errno == errno.ESRCH: # errno 3: No such process
        # assume tor has already exited due to SIGNAL HALT
        logging.warn("Tor has already exited")
    else:
        raise
