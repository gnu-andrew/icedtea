package org.classpath.icedtea.plugin;

import sun.applet.PluginCallRequest;
import sun.applet.PluginCallRequestFactory;

public class PluginCallRequestFactoryImpl implements PluginCallRequestFactory {

	public PluginCallRequest getPluginCallRequest(String id, String message, String returnString) {

		if (id == "member") {
			return new GetMemberPluginCallRequest(message, returnString);
		} else if (id == "void") {
			return new VoidPluginCallRequest(message, returnString);
		} else if (id == "window") {
			return new GetWindowPluginCallRequest(message, returnString);
		} else {
			throw new RuntimeException ("Unknown plugin call request type requested from factory");
		}
		
	}

}
