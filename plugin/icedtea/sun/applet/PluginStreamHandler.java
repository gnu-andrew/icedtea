package sun.applet;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.StreamTokenizer;
import java.net.MalformedURLException;
import java.nio.charset.Charset;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.LinkedList;
import java.util.StringTokenizer;


public class PluginStreamHandler {

    private BufferedReader pluginInputReader;
    private StreamTokenizer pluginInputTokenizer;
    private BufferedWriter pluginOutputWriter;
    
    private RequestQueue queue = new RequestQueue();

	LinkedList<String> writeQueue = new LinkedList<String>();

	PluginMessageConsumer consumer;
	Boolean shuttingDown = false;
	
	PluginAppletViewer pav;
	
    public PluginStreamHandler(InputStream inputstream, OutputStream outputstream)
    throws MalformedURLException, IOException
    {

    	System.err.println("Current context CL=" + Thread.currentThread().getContextClassLoader());
    	try {
			pav = (PluginAppletViewer) ClassLoader.getSystemClassLoader().loadClass("sun.applet.PluginAppletViewer").newInstance();
			System.err.println("Loaded: " + pav + " CL=" + pav.getClass().getClassLoader());
		} catch (InstantiationException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (ClassNotFoundException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

    	System.err.println("Creating consumer...");
    	consumer = new PluginMessageConsumer(this);

    	// Set up input and output pipes.  Use UTF-8 encoding.
    	pluginInputReader =
    		new BufferedReader(new InputStreamReader(inputstream,
    				Charset.forName("UTF-8")));
    	pluginInputTokenizer = new StreamTokenizer(pluginInputReader);
    	pluginInputTokenizer.resetSyntax();
    	pluginInputTokenizer.whitespaceChars('\u0000', '\u0000');
    	pluginInputTokenizer.wordChars('\u0001', '\u00FF');
    	pluginOutputWriter =
    		new BufferedWriter(new OutputStreamWriter
    				(outputstream, Charset.forName("UTF-8")));

    	/*
	while(true) {
            String message = read();
            PluginDebug.debug(message);
            handleMessage(message);
            // TODO:
            // write(queue.peek());
	}
    	 */
    }

    public void startProcessing() {

    	Thread listenerThread = new Thread() {

    		public void run() {
    			
    			while (true) {

    				System.err.println("Waiting for data...");

    				int readChar = -1;
    				// blocking read, discard first character
    				try {
    					readChar = pluginInputReader.read();
    				} catch (IOException ioe) {
    					// plugin may have detached
    				}

    				// if not disconnected
    				if (readChar != -1) {
    					String s = read();
    					System.err.println("Got data, consuming " + s);
    					consumer.consume(s);
    				} else {
    					try {
    						// Close input/output channels to plugin.
    						pluginInputReader.close();
    						pluginOutputWriter.close();
    					} catch (IOException exception) {
    						// Deliberately ignore IOException caused by broken
    						// pipe since plugin may have already detached.
    					}
    					AppletSecurityContextManager.dumpStore(0);
    					System.err.println("APPLETVIEWER: exiting appletviewer");
    					System.exit(0);
    				}
    			}
    		}
    	};
    	
    	listenerThread.start();
    }
    
    public void postMessage(String s) {

    	if (s == null || s.equals("shutdown")) {
    	    try {
    		// Close input/output channels to plugin.
    		pluginInputReader.close();
    		pluginOutputWriter.close();
    	    } catch (IOException exception) {
    		// Deliberately ignore IOException caused by broken
    		// pipe since plugin may have already detached.
    	    }
    	    AppletSecurityContextManager.dumpStore(0);
    	    System.err.println("APPLETVIEWER: exiting appletviewer");
    	    System.exit(0);
    	}

   		//PluginAppletSecurityContext.contexts.get(0).store.dump();
   		PluginDebug.debug("Plugin posted: " + s);

		System.err.println("Consuming " + s);
		consumer.consume(s);

   		PluginDebug.debug("Added to queue");
    }
    
    public void handleMessage(String message) throws PluginException {

    	StringTokenizer st = new StringTokenizer(message, " ");

    	String type = st.nextToken();
    	final int identifier = Integer.parseInt(st.nextToken());

    	String rest = "";
    	int reference = -1;
    	String src = null;

    	String potentialReference = st.hasMoreTokens() ? st.nextToken() : "";

    	// if the next token is reference, read it, else reset "rest"
    	if (potentialReference.equals("reference")) {
    		reference = Integer.parseInt(st.nextToken());
    	} else {
    		rest += potentialReference + " ";
    	}

    	String potentialSrc = st.hasMoreTokens() ? st.nextToken() : "";

    	if (potentialSrc.equals("src")) {
    		src = st.nextToken();
    	} else {
    		rest += potentialSrc + " ";
    	}

    	while (st.hasMoreElements()) {
    		rest += st.nextToken();
    		rest += " ";
    	}

    	rest = rest.trim();

    	try {

    		System.err.println("Breakdown -- type: " + type + " identifier: " + identifier + " reference: " + reference + " src: " + src + " rest: " + rest);

    		if (rest.contains("JavaScriptGetWindow")
    				|| rest.contains("JavaScriptGetMember")
    				|| rest.contains("JavaScriptSetMember")
    				|| rest.contains("JavaScriptGetSlot")
    				|| rest.contains("JavaScriptSetSlot")
    				|| rest.contains("JavaScriptEval")
    				|| rest.contains("JavaScriptRemoveMember")
    				|| rest.contains("JavaScriptCall")
    				|| rest.contains("JavaScriptFinalize")
    				|| rest.contains("JavaScriptToString")) {
    			
				finishCallRequest(rest);
    			return;
    		}

    		final int freference = reference;
    		final String frest = rest;

    		if (type.equals("instance")) {
    			PluginAppletViewer.handleMessage(identifier, freference,frest);
    		} else if (type.equals("context")) {
    			PluginDebug.debug("Sending to PASC: " + identifier + "/" + reference + " and " + rest);
    			AppletSecurityContextManager.handleMessage(identifier, src, reference, rest);
    		}
    	} catch (Exception e) {
    		throw new PluginException(this, identifier, reference, e);
    	}
    }

    public void postCallRequest(PluginCallRequest request) {
        synchronized(queue) {
   			queue.post(request);
        }
    }

    private void finishCallRequest(String message) {
    	PluginDebug.debug ("DISPATCHCALLREQUESTS 1");
    	synchronized(queue) {
    		PluginDebug.debug ("DISPATCHCALLREQUESTS 2");
    		PluginCallRequest request = queue.pop();

    		// make sure we give the message to the right request 
    		// in the queue.. for the love of God, MAKE SURE!

    		// first let's be efficient.. if there was only one 
    		// request in queue, we're already set
    		if (queue.size() != 0) {

    			int size = queue.size();
    			int count = 0;

    			while (!request.serviceable(message)) {

    				// something is very wrong.. we have a message to 
    				// process, but no one to service it
    				if (count >= size) {
    					throw new RuntimeException("Unable to find processor for message " + message);
    				}

    				// post request at the end of the queue
    				queue.post(request);

    				// Look at the next request
    				request = queue.pop();

    				count++;
    			}

    		}

    		PluginDebug.debug ("DISPATCHCALLREQUESTS 3");
    		if (request != null) {
    			PluginDebug.debug ("DISPATCHCALLREQUESTS 5");
    			synchronized(request) {
    				request.parseReturn(message);
    				request.notifyAll();
    			}
    			PluginDebug.debug ("DISPATCHCALLREQUESTS 6");
    			PluginDebug.debug ("DISPATCHCALLREQUESTS 7");
    		}
    	}
    	PluginDebug.debug ("DISPATCHCALLREQUESTS 8");
    }

    /**
     * Read string from plugin.
     *
     * @return the read string
     *
     * @exception IOException if an error occurs
     */
    private String read()
    {
    	try {
    		pluginInputTokenizer.nextToken();
    	} catch (IOException e) {
    		throw new RuntimeException(e);
    	}
    	String message = pluginInputTokenizer.sval;
    	PluginDebug.debug("  PIPE: appletviewer read: " + message);
    	if (message == null || message.equals("shutdown")) {
    		synchronized(shuttingDown) {
    			shuttingDown = true;
    		}
    		try {
    			// Close input/output channels to plugin.
    			pluginInputReader.close();
    			pluginOutputWriter.close();
    		} catch (IOException exception) {
    			// Deliberately ignore IOException caused by broken
    			// pipe since plugin may have already detached.
    		}
    		AppletSecurityContextManager.dumpStore(0);
    		System.err.println("APPLETVIEWER: exiting appletviewer");
    		System.exit(0);
    	}
    	return message;
    }
    
    /**
     * Write string to plugin.
     * 
     * @param message the message to write
     *
     * @exception IOException if an error occurs
     */
    public void write(String message)
    {

    	System.err.println("  PIPE: appletviewer wrote: " + message);
        synchronized(pluginOutputWriter) {
        	try {
        		pluginOutputWriter.write(message, 0, message.length());
        		pluginOutputWriter.write(0);
        		pluginOutputWriter.flush();
        	} catch (IOException e) {
        		// if we are shutting down, ignore write failures as 
        		// pipe may have closed
        		synchronized(shuttingDown) {
        			if (!shuttingDown) {
        				e.printStackTrace();
        			}
        		}

        		// either ways, if the pipe is broken, there is nothing 
        		// we can do anymore. Don't hang around.
        		System.err.println("Unable to write to PIPE. APPLETVIEWER exiting");        		
        		System.exit(1);
        	}
		}

		return;
    /*	
    	synchronized(writeQueue) {
            writeQueue.add(message);
            System.err.println("  PIPE: appletviewer wrote: " + message);
    	}
	*/

    }
    
    public boolean messageAvailable() {
    	return writeQueue.size() != 0;
    }

    public String getMessage() {
    	synchronized(writeQueue) {
			String ret = writeQueue.size() > 0 ? writeQueue.poll() : "";
    		return ret;
    	}
    }
}
