#! /usr/bin/env python3

import sys
import logging
# logging.basicConfig( stream = sys.stderr )
logger = logging.getLogger()

import time
import re
from os.path import commonprefix
from ipykernel.kernelbase import Kernel
from subprocess import check_output, PIPE, Popen
from threading import Thread
from queue import Queue, Empty

from pygments import highlight
from pygments.lexers import HuginnLexer
from pygments.formatters import HtmlFormatter

import locale
locale.setlocale( locale.LC_ALL, "POSIX" )
locale.setlocale( locale.LC_CTYPE, "pl_PL.UTF-8" )
locale.setlocale( locale.LC_COLLATE, "pl_PL.UTF-8" )
locale.setlocale( locale.LC_TIME, "en_DK.UTF-8" )

def isword( x ):
	return x.isalnum() or ( x == '_' )

def markdown( x ):
	return x.replace( "\n", "  \n" )

def plain( x ):
	return x.replace( "\\", "" )

class IHuginnKernel( Kernel ):
	implementation = "IHuginn"
	language = "Huginn"
	language_info = {
		"name": "Huginn",
		"mimetype": "text/x-huginn",
		"file_extension": ".hgn",
		"codemirror_mode": "huginn"
	}

	def enqueue_output( out_, queue_ ):
		for line in iter( out_.readline, b'' ):
			queue_.put( line )
		out_.close()

	def __init__( self_, session, profile_dir, **kwArgs ):
#		logger.warning( "Starting Huginn kernel" )
		Kernel.__init__( self_, session = session, profile_dir = profile_dir, **kwArgs )
		self_._sessionName = session.session
		self_._profileDir = profile_dir.location
		ver = check_output( [ "huginn", "--version", "-v" ] ).decode( "utf-8" )
		self_.banner = "Jupyter kernel IHuginn for Huginn programming language.\n" + ver.strip()
		sver = ver.split( "\n" )
		self_.language_version = sver[1].split( " " )[1]
		self_.implementation_version = sver[0].split( " " )[1]
		conf = check_output( [ "huginn", "-W" ] ).decode( "utf-8" ).strip().split( "\n" )
		self_._historyFile = None
		self_._history = []
		for line in conf:
			item = line.split( " " )
			if item[0] == "history-file":
				self_._historyFile = item[1].strip( "\"" )
				break
		self_.do_start()

	def do_start( self_ ):
		if self_._historyFile:
			try:
				with open( self_._historyFile ) as f:
					self_._history = [( None, None, line.strip() ) for line in f]
			except:
				pass
		ON_POSIX = 'posix' in sys.builtin_module_names
		self_._huginn = Popen(
			[ "huginn", "--jupyter", "--no-default-init", "--session=jupyter-session-{}.hgn".format( self_._sessionName ) ],
			stdin = PIPE,
			stdout = PIPE,
			stderr = PIPE,
			bufsize = 1,
			universal_newlines = True,
			close_fds = ON_POSIX
		)
		self_._stdoutQueue = Queue()
		self_._stdoutThread = Thread( target = IHuginnKernel.enqueue_output, args = ( self_._huginn.stdout, self_._stdoutQueue ) )
		self_._stdoutThread.start()
		self_._stderrQueue = Queue()
		self_._stderrThread = Thread( target = IHuginnKernel.enqueue_output, args = ( self_._huginn.stderr, self_._stderrQueue ) )
		self_._stderrThread.start()

	def read_output( self_ ):
		output = ""
		status = ""
		while True:
			line = self_._stdoutQueue.get().rstrip()
			if line.startswith( "// " ):
				status = line[3:]
				break
			output += ( line + "\n" )
		return output, status

	def do_execute( self_, code, silent, store_history = True, user_expressions = None, allow_stdin = False ):
		"""
		logger.warning(
			"code = " + ( code )
			+ ", silent = " + str( silent )
			+ ", store_history = " + str( store_history )
			+ ", user_expressions = " + str( user_expressions )
			+ ", allow_stdin = " + str( allow_stdin )
		)
		"""
		code = code.strip()
		if not code:
			return { "status": "ok", "execution_count": self_.execution_count, "payload": [], "user_expressions": {} }

		magicResult = None
		isMagic = code[0] == '%'
		if isMagic:
			magicResult = self_.do_magic( code[1:] )
		else:
			self_._huginn.stdin.write( code + "\n//\n" )
		if magicResult != None:
			return magicResult

		output, status = self_.read_output()

		err = ""
		while not self_._stderrQueue.empty():
			err += self_._stderrQueue.get_nowait()

		# Return results.
		output = output.rstrip()
		if not silent and ( status == "ok" ) and output:
			data = None
			if isMagic:
				data = {
					"text/markdown": markdown( output ),
					"text/plain": plain( output )
				}
			else:
				data = {
					"text/x-huginn": output,
					"text/markdown": output,
					"text/html": highlight( output, HuginnLexer(), HtmlFormatter() ),
					"text/plain": output
				}
			streamContent = {
				"execution_count": self_.execution_count,
				"data": data,
				"metadata": {}
			}
			self_.send_response( self_.iopub_socket, "execute_result", streamContent )
		output += err
		if err or ( status == "error" ):
			streamContent = { "name": "stderr", "text": output }
			self_.send_response( self_.iopub_socket, "stream", streamContent )

		if not err:
			if store_history:
				self_._history.append( ( None, None, code ) )
				if self_._historyFile:
					with open( self_._historyFile, "a" ) as f:
						f.write( code + "\n" )
			return { "status": "ok", "execution_count": self_.execution_count, "payload": [], "user_expressions": {} }
		else:
			return { "status": "err", "execution_count": self_.execution_count }

	def do_magic( self_, magic ):
		forward = set( [
			"declarations", "doc", "imports", "load",
			"lsmagic", "reset", "set", "source", "time", "variables"
		] )
		statusOk = True
		data = ""
		if magic == "version":
			data = self_.banner
		elif magic.split()[ 0 ] in forward:
			self_._huginn.stdin.write( "//" + magic + "\n" )
			return None
		else:
			statusOk = False
			data = "Unknown magic: %" + magic
		if statusOk:
			streamContent = {
				"execution_count": self_.execution_count,
				"data": {
					"text/markdown": markdown( data ),
					"text/plain": plain( data )
				},
				"metadata": {}
			}
			self_.send_response( self_.iopub_socket, "execute_result", streamContent )
			return { "status": "ok", "execution_count": self_.execution_count, "payload": [], "user_expressions": {} }
		else:
			self_.send_response( self_.iopub_socket, "stream", { "name": "stderr", "text": data } )
			return { "status": "err", "execution_count": self_.execution_count }

	def do_shutdown( self_, restart ):
		try:
			self_._huginn.stdin.close()
			self_._huginn.terminate()
			self_._huginn.kill()
			self_._huginn.wait()
		except Execption as e:
			logger.warning( "Exception: " + type( e ) + " occured: " + str( e ) )
		self_._huginn = None
		with self_._stdoutQueue.mutex:
			self_._stdoutQueue.queue.clear()
		self_._stdoutQueue.join()
		self_._stdoutQueue = None
		self_._stdoutThread.join()
		self_._stdoutThread = None
		with self_._stderrQueue.mutex:
			self_._stderrQueue.queue.clear()
		self_._stderrQueue.join()
		self_._stderrQueue = None
		self_._stderrThread.join()
		self_._stderrThread = None
		self_._history = []
		if restart:
			self_.do_start()
		return { "status": "ok", "restart": restart }

	def do_complete( self_, code, cursor_pos ):
		wordBreakCharacters = " \t\n\"\\'`@$><=?:;,|&![{()}]+-*/%^~"
		s = cursor_pos - 1
		while s >= 0:
			if wordBreakCharacters.find( code[s] ) >= 0:
				break
			s -= 1
		if ( s < 0 ) or ( code[s] != '\\' ):
			s += 1
		call = code[s:cursor_pos]
		word = ""
		symbolic = False
		if len( call ) > 0 and call[0] == '\\':
			symbolic = True
			word = call
		else:
			symbol = re.split( "\W+", call )
			word = symbol[-1]
			call = ""
			if len( symbol ) > 1:
				s += ( len( symbol[0] ) + 1 )
				call = symbol[0]

		self_._huginn.stdin.write( "//?" + call + "\n" )
		output = self_.read_output()[0].rstrip()

		compl = []

		for c in output.split( "\n" ):
			if symbolic or c.startswith( word ):
				compl.append( c )

# Perform immediate completion on unambiguous part.
		if len( compl ) > 1:
			cp = commonprefix( compl )
			if len( cp ) > len( word ):
				compl = [ cp ]

#		logger.warning( "[{}, {}, {}, {}, {}, {}]".format( code, call, word, compl, s, cursor_pos ) )
		return { "matches": compl, "cursor_start": s, "cursor_end": cursor_pos, "metadata": {}, "status": "ok" }

	def do_inspect( self_, code, cursor_pos, detail_level = 0 ):
		word = ""
		i = cursor_pos
		while ( i > 0 ) and isword( code[i - 1] ):
			i -= 1
			word = code[i] + word
		l = len( code )
		while ( cursor_pos < l ) and isword( code[cursor_pos] ):
			word += code[cursor_pos]
			cursor_pos += 1
		self_._huginn.stdin.write( "// doc " + word + "\n" )
		output, _ = self_.read_output()
#		logger.warning( "[{}, {}]".format( word, output ) )
		return { "found": True, "data": { "text/plain": plain( output ), "text/markdown": markdown( output ) }, "metadata": {}, "status": "ok" }

	def do_history( self_, hist_access_type, output, raw, session = None, start = None, stop = None, n = None, pattern = None, unique = False ):
		"""
		logger.warning(
			"hist_access_type = " + str( hist_access_type )
			+ ", output = " + str( output )
			+ ", raw = " + str( raw )
			+ ", session = " + str( session )
			+ ", start = " + str( start )
			+ ", stop = " + str( stop )
			+ ", n = " + str( n )
			+ ", pattern = " + str( pattern )
			+ ", unique = " + str( unique ) )
		"""
		return { "history": self_._history }

if __name__ == '__main__':
	from ipykernel.kernelapp import IPKernelApp
	IPKernelApp.launch_instance( kernel_class = IHuginnKernel )

