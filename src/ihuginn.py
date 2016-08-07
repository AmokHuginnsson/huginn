#! /usr/bin/env python3

import sys
import logging
import time
from ipykernel.kernelbase import Kernel
from subprocess import check_output, PIPE, Popen
from threading import Thread
from queue import Queue, Empty

logger = logging.getLogger()

class IHuginnKernel( Kernel ):
	implementation = "IHuginn"
	language = "Huginn"
	language_info = {
		"mimetype": "text/x-huginn",
		"file_extension": "hgn"
	}

	def enqueue_output( out_, queue_ ):
		for line in iter( out_.readline, b'' ):
			queue_.put( line )
		out_.close()

	def __init__( self_, **kwargs ):
#		logger.warning( "Starting Huginn kernel" )
		Kernel.__init__( self_, **kwargs )
		ver = check_output( [ "huginn", "--version", "-v" ] ).decode( "utf-8" )
		self_.banner = "Jupyter kernel IHuginn for Huginn programming language.\n" + ver
		sver = ver.split( "\n" )
		self_.language_version = sver[1].split( " " )[1]
		self_.implementation_version = sver[0].split( " " )[1]
		ON_POSIX = 'posix' in sys.builtin_module_names
		self_._huginn = Popen( ["huginn", "-i"], stdin = PIPE, stdout = PIPE, stderr = PIPE, bufsize = 1, universal_newlines = True, close_fds = ON_POSIX )
		self_._stdoutQueue = Queue()
		self_._stderrQueue = Queue()
		t = Thread( target = IHuginnKernel.enqueue_output, args = ( self_._huginn.stdout, self_._stdoutQueue ) )
		t.daemon = True # thread dies with the program
		t.start()
		t = Thread( target = IHuginnKernel.enqueue_output, args = ( self_._huginn.stderr, self_._stderrQueue ) )
		t.daemon = True # thread dies with the program
		t.start()

	def do_execute( self_, code_, silent_, storeHistory_ = True, userExpressions_ = None, allowStdin_ = False ):
		code_ = code_.strip()
		if not code_:
			return { "status": "ok", "execution_count": self_.execution_count, "payload": [], "user_expressions": {} }
		while not self_._stdoutQueue.empty():
			line = self_._stdoutQueue.get_nowait()
		self_._huginn.stdin.write( code_ + "\n" )

		output = ""
		line = "<timeout>"
		err = ""
		timeout = 3.
		while ( len( line ) > 0 ) or ( timeout > 0. ):
			try:
				line = self_._stdoutQueue.get_nowait() # or q.get(timeout=.1)
			except Empty:
				line = ""
				if timeout > 0.:
					time.sleep( 0.01 )
					timeout -= 0.01
			else: # got line
				line = line.strip()
				if len( line ) > 0:
					output += line + "\n"
					timeout = 0.
		while not self_._stderrQueue.empty():
			err += self_._stderrQueue.get_nowait()

		# Return results.
		if not silent_:
			streamContent = { "name": "stdout", "text": output }
			self_.send_response( self_.iopub_socket, "stream", streamContent )
		if err:
			streamContent = { "name": "stderr", "text": err }
			self_.send_response( self_.iopub_socket, "stream", streamContent )

		if not err:
			return { "status": "ok", "execution_count": self_.execution_count, "payload": [], "user_expressions": {} }
		else:
			return { "status": "err", "execution_count": self_.execution_count }

if __name__ == '__main__':
	from ipykernel.kernelapp import IPKernelApp
	IPKernelApp.launch_instance( kernel_class = IHuginnKernel )

