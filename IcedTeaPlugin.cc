/* IcedTeaPlugin -- implement OJI
   Copyright (C) 2008  Red Hat

This file is part of IcedTea.

IcedTea is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

IcedTea is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with IcedTea; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */

#include <nsStringAPI.h>

PRThread* current_thread ();

#if PR_BYTES_PER_LONG == 8
#define PLUGIN_JAVASCRIPT_TYPE jlong
#define PLUGIN_INITIALIZE_JAVASCRIPT_ARGUMENT(args, obj) args[0].j = obj
#define PLUGIN_JAVASCRIPT_SIGNATURE "(J)V"
#else
#define PLUGIN_JAVASCRIPT_TYPE jint
#define PLUGIN_INITIALIZE_JAVASCRIPT_ARGUMENT(args, obj) args[0].i = obj
#define PLUGIN_JAVASCRIPT_SIGNATURE "(I)V"
#endif

// System includes.
#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// GTK includes.
#include <gtk/gtk.h>

// FIXME: Look into this:
// #0  nsACString_internal (this=0xbff3016c) at ../../../dist/include/string/nsTSubstring.h:522
// #1  0x007117c9 in nsDependentCSubstring (this=0xbff3016c, str=@0xab20d00, startPos=0, length=0) at ../../dist/include/string/nsTDependentSubstring.h:68
// #2  0x0076a9d9 in Substring (str=@0xab20d00, startPos=0, length=0) at ../../dist/include/string/nsTDependentSubstring.h:103
// #3  0x008333a7 in nsStandardURL::Hostport (this=0xab20ce8) at nsStandardURL.h:338
// #4  0x008299b8 in nsStandardURL::GetHostPort (this=0xab20ce8, result=@0xbff30210) at nsStandardURL.cpp:1003
// #5  0x0095b9dc in nsPrincipal::GetOrigin (this=0xab114e0, aOrigin=0xbff30320) at nsPrincipal.cpp:195
// #6  0x0154232c in nsCSecurityContext::GetOrigin (this=0xab8f410, buf=0xbff30390 "\004", buflen=256) at nsCSecurityContext.cpp:126
// #7  0x04db377e in CNSAdapter_SecurityContextPeer::GetOrigin () from /opt/jdk1.6.0_03/jre/plugin/i386/ns7/libjavaplugin_oji.so
// #8  0x05acd59f in getAndPackSecurityInfo () from /opt/jdk1.6.0_03/jre/lib/i386/libjavaplugin_nscp.so
// #9  0x05acc77f in jni_SecureCallMethod () from /opt/jdk1.6.0_03/jre/lib/i386/libjavaplugin_nscp.so
// #10 0x05aba88d in CSecureJNIEnv::CallMethod () from /opt/jdk1.6.0_03/jre/lib/i386/libjavaplugin_nscp.so
// #11 0x04db1be7 in CNSAdapter_SecureJNIEnv::CallMethod () from /opt/jdk1.6.0_03/jre/plugin/i386/ns7/libjavaplugin_oji.so
// #12 0x0153e62f in ProxyJNIEnv::InvokeMethod (env=0xa8b8040, obj=0x9dad690, method=0xa0ed070, args=0x0) at ProxyJNI.cpp:571
// #13 0x0153f91c in ProxyJNIEnv::InvokeMethod (env=0xa8b8040, obj=0x9dad690, method=0xa0ed070, args=0xbff3065c "\235\225$") at ProxyJNI.cpp:580
// #14 0x0153fdbf in ProxyJNIEnv::CallObjectMethod (env=0xa8b8040, obj=0x9dad690, methodID=0xa0ed070) at ProxyJNI.cpp:641

#define NOT_IMPLEMENTED() \
  printf ("NOT IMPLEMENTED: %s\n", __PRETTY_FUNCTION__)

#define ID(object) \
  (object == NULL ? (PRUint32) 0 : reinterpret_cast<JNIReference*> (object)->identifier)

// Tracing.
class Trace
{
public:
  Trace (char const* name, char const* function)
  {
    Trace::name = name;
    Trace::function = function;
    printf ("ICEDTEA PLUGIN: %s%s\n",
             name, function);
  }

  ~Trace ()
  {
    printf ("ICEDTEA PLUGIN: %s%s %s\n",
             name, function, "return");
  }
private:
  char const* name;
  char const* function;
};

#if 1
// Debugging macros.
#define PLUGIN_DEBUG(message)                                           \
  printf ("ICEDTEA PLUGIN: %s\n", message)

#define PLUGIN_DEBUG_TWO(first, second)                                 \
  printf ("ICEDTEA PLUGIN: %s %s\n",      \
           first, second)

// Testing macro.
#define PLUGIN_TEST(expression, message)  \
  do                                            \
    {                                           \
      if (!(expression))                        \
        printf ("FAIL: %d: %s\n", __LINE__,     \
                message);                       \
    }                                           \
  while (0);

// __func__ is a variable, not a string literal, so it cannot be
// concatenated by the preprocessor.
#define PLUGIN_TRACE_JNIENV() Trace _trace ("JNIEnv::", __func__)
#define PLUGIN_TRACE_FACTORY() Trace _trace ("Factory::", __func__)
#define PLUGIN_TRACE_INSTANCE() Trace _trace ("Instance::", __func__)
#define PLUGIN_TRACE_EVENTSINK() Trace _trace ("EventSink::", __func__)
#define PLUGIN_TRACE_LISTENER() Trace _trace ("Listener::", __func__)
//#define PLUGIN_TRACE_RC() Trace _trace ("ResultContainer::", __func__)
#define PLUGIN_TRACE_RC()

// Error reporting macros.
#define PLUGIN_ERROR(message)                                       \
  fprintf (stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__,  \
           message)

#define PLUGIN_ERROR_TWO(first, second)                                 \
  fprintf (stderr, "%s:%d: Error: %s: %s\n", __FILE__, __LINE__,  \
           first, second)

#define PLUGIN_ERROR_THREE(first, second, third)                        \
  fprintf (stderr, "%s:%d: Error: %s: %s: %s\n", __FILE__,        \
           __LINE__, first, second, third)

#define PLUGIN_CHECK_RETURN(message, result)           \
  if (NS_SUCCEEDED (result))                    \
    PLUGIN_DEBUG (message);                     \
  else                                          \
    {                                           \
      PLUGIN_ERROR (message);                   \
      return result;                            \
    }

#define PLUGIN_CHECK(message, result)           \
  if (NS_SUCCEEDED (result))                    \
    PLUGIN_DEBUG (message);                     \
  else                                          \
    PLUGIN_ERROR (message);

#else

// Debugging macros.
#define PLUGIN_DEBUG(message)
#define PLUGIN_DEBUG_TWO(first, second)

// Testing macros.
#define PLUGIN_TEST(expression, message)
#define PLUGIN_TRACE_JNIENV()
#define PLUGIN_TRACE_FACTORY() Trace _trace ("Factory::", __func__)
//#define PLUGIN_TRACE_FACTORY()
#define PLUGIN_TRACE_INSTANCE()
#define PLUGIN_TRACE_EVENTSINK()
#define PLUGIN_TRACE_LISTENER()

// Error reporting macros.
#define PLUGIN_ERROR(message)                                       \
  fprintf (stderr, "%s:%d: Error: %s\n", __FILE__, __LINE__,  \
           message)

#define PLUGIN_ERROR_TWO(first, second)                                 \
  fprintf (stderr, "%s:%d: Error: %s: %s\n", __FILE__, __LINE__,  \
           first, second)

#define PLUGIN_ERROR_THREE(first, second, third)                        \
  fprintf (stderr, "%s:%d: Error: %s: %s: %s\n", __FILE__,        \
           __LINE__, first, second, third)
#define PLUGIN_CHECK_RETURN(message, result)
#define PLUGIN_CHECK(message, result)
#endif

#define PLUGIN_NAME "IcedTea Web Browser Plugin"
#define PLUGIN_DESCRIPTION "The " PLUGIN_NAME " executes Java applets."
#define PLUGIN_MIME_DESC                                               \
  "application/x-java-vm:class,jar:IcedTea;"                           \
  "application/x-java-applet:class,jar:IcedTea;"                       \
  "application/x-java-applet;version=1.1:class,jar:IcedTea;"           \
  "application/x-java-applet;version=1.1.1:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.1.2:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.1.3:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.2:class,jar:IcedTea;"           \
  "application/x-java-applet;version=1.2.1:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.2.2:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.3:class,jar:IcedTea;"           \
  "application/x-java-applet;version=1.3.1:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.4:class,jar:IcedTea;"           \
  "application/x-java-applet;version=1.4.1:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.4.2:class,jar:IcedTea;"         \
  "application/x-java-applet;version=1.5:class,jar:IcedTea;"           \
  "application/x-java-applet;version=1.6:class,jar:IcedTea;"           \
  "application/x-java-applet;version=1.7:class,jar:IcedTea;"           \
  "application/x-java-applet;jpi-version=1.7.0_00:class,jar:IcedTea;"  \
  "application/x-java-bean:class,jar:IcedTea;"                         \
  "application/x-java-bean;version=1.1:class,jar:IcedTea;"             \
  "application/x-java-bean;version=1.1.1:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.1.2:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.1.3:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.2:class,jar:IcedTea;"             \
  "application/x-java-bean;version=1.2.1:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.2.2:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.3:class,jar:IcedTea;"             \
  "application/x-java-bean;version=1.3.1:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.4:class,jar:IcedTea;"             \
  "application/x-java-bean;version=1.4.1:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.4.2:class,jar:IcedTea;"           \
  "application/x-java-bean;version=1.5:class,jar:IcedTea;"             \
  "application/x-java-bean;version=1.6:class,jar:IcedTea;"             \
  "application/x-java-bean;version=1.7:class,jar:IcedTea;"             \
  "application/x-java-bean;jpi-version=1.7.0_00:class,jar:IcedTea;"

#define FAILURE_MESSAGE "IcedTeaPluginFactory error: Failed to run %s." \
  "  For more detail rerun \"firefox -g\" in a terminal window."

// Global instance counter.
// A global variable for reporting GLib errors.  This must be free'd
// and set to NULL after each use.
static GError* channel_error = NULL;
// Fully-qualified appletviewer executable.
static char* appletviewer_executable = NULL;
static char* libjvm_so = NULL;

class IcedTeaPluginFactory;

static PRBool factory_created = PR_FALSE;
static IcedTeaPluginFactory* factory = NULL;

#include <nspr.h>

#include <prtypes.h>

// IcedTeaJNIEnv helpers.
class JNIReference
{
public:
  JNIReference (PRUint32 identifier);
  ~JNIReference ();
  PRUint32 identifier;
  PRUint32 count;
};

JNIReference::JNIReference (PRUint32 identifier)
  : identifier (identifier),
    count (0)
{
  printf ("JNIReference CONSTRUCT: %d %p\n", identifier, this);
}

JNIReference::~JNIReference ()
{
  printf ("JNIReference DECONSTRUCT: %d %p\n", identifier, this);
}

class JNIID : public JNIReference
{
public:
  JNIID (PRUint32 identifier, char const* signature);
  ~JNIID ();
  char const* signature;
};

JNIID::JNIID (PRUint32 identifier, char const* signature)
  : JNIReference (identifier),
    signature (strdup (signature))
{
  printf ("JNIID CONSTRUCT: %d %p\n", identifier, this);
}

JNIID::~JNIID ()
{
  printf ("JNIID DECONSTRUCT: %d %p\n", identifier, this);
}

char const* TYPES[10] = { "Object",
                          "boolean",
                          "byte",
                          "char",
                          "short",
                          "int",
                          "long",
                          "float",
                          "double",
                          "void" };

#include <nsIThread.h>

// FIXME: create index from security context.
#define MESSAGE_CREATE(reference)                            \
  const char* addr; \
  char context[16]; \
  GetCurrentPageAddress(&addr); \
  GetCurrentContextAddr(context); \
  nsCString message ("context ");                            \
  message.AppendInt (0);                                     \
  message += " reference ";                                  \
  message.AppendInt (reference);                             \
  if (factory->codebase_map.find(nsCString(addr)) != factory->codebase_map.end()) \
  { \
	  message += " src "; \
	  message += factory->codebase_map[nsCString(addr)];\
  } \
  message += " ";											 \
  message += __func__;                                       \
  if (factory->result_map[reference] == NULL) {                \
	   factory->result_map[reference] = new ResultContainer();  \
	   printf("ResultMap created -- %p %d\n", factory->result_map[reference], factory->result_map[reference]->returnIdentifier); \
  } \
  else                                                      \
	   factory->result_map[reference]->Clear(); 


#define MESSAGE_ADD_STRING(name)                \
  message += " ";                               \
  message += name;

#define MESSAGE_ADD_SIZE(size)                  \
  message += " ";                               \
  message.AppendInt (size);

// Pass character value through socket as an integer.
#define MESSAGE_ADD_TYPE(type)                  \
  message += " ";                               \
  message += TYPES[type];

#define MESSAGE_ADD_REFERENCE(clazz)                    \
  message += " ";                                       \
  message.AppendInt (clazz ? ID (clazz) : 0);

#define MESSAGE_ADD_ID(id)                                              \
  message += " ";                                                       \
  message.AppendInt (reinterpret_cast<JNIID*> (id)->identifier);

#define MESSAGE_ADD_ARGS(id, args)               \
  message += " ";                                \
  char* expandedArgs = ExpandArgs (reinterpret_cast<JNIID*> (id), args); \
  message += expandedArgs;                                              \
  free (reinterpret_cast<void*> (expandedArgs));                        \
  expandedArgs = NULL;
 

#define MESSAGE_ADD_VALUE(id, val)                      \
  message += " ";                                       \
  char* expandedValues =                                \
    ExpandArgs (reinterpret_cast<JNIID*> (id), &val);   \
  message += expandedValues;                            \
  free (expandedValues);                                \
  expandedValues = NULL;

#define MESSAGE_ADD_STRING_UCS(pointer, length) \
  for (int i = 0; i < length; i++)              \
    {                                           \
      message += " ";                           \
      message.AppendInt (pointer[i]);           \
    }

#define MESSAGE_ADD_STRING_UTF(pointer)         \
  int i = 0;                                    \
  while (pointer[i] != 0)                       \
    {                                           \
      message += " ";                           \
      message.AppendInt (pointer[i]);           \
      i++;                                      \
    }

#define MESSAGE_SEND()                          \
  factory->SendMessageToAppletViewer (message);

// FIXME: Right now, the macro below will exit only 
// if error occured and we are in the middle of a 
// shutdown (so that the permanent loop does not block 
// proper exit). We need better error handling

#define PROCESS_PENDING_EVENTS_REF(reference) \
    if (factory->shutting_down == PR_TRUE && \
		factory->result_map[reference]->errorOccured == PR_TRUE) \
	{                                                           \
		printf("Error occured. Exiting function\n");            \
		return NS_ERROR_FAILURE; \
	} \
	PRBool hasPending;  \
	factory->current->HasPendingEvents(&hasPending); \
	if (hasPending == PR_TRUE) { \
		PRBool processed = PR_FALSE; \
		factory->current->ProcessNextEvent(PR_TRUE, &processed); \
	} else { \
		PR_Sleep(PR_INTERVAL_NO_WAIT); \
	}

#define PROCESS_PENDING_EVENTS \
	PRBool hasPending;  \
	factory->current->HasPendingEvents(&hasPending); \
	if (hasPending == PR_TRUE) { \
		PRBool processed = PR_FALSE; \
		factory->current->ProcessNextEvent(PR_TRUE, &processed); \
	} else { \
		PR_Sleep(PR_INTERVAL_NO_WAIT); \
	}

#define MESSAGE_RECEIVE_REFERENCE(reference, cast, name)                \
  nsresult res = NS_OK;                                                 \
  printf ("RECEIVE 1\n");                                               \
  while (factory->result_map[reference]->returnIdentifier == -1 &&\
	     factory->result_map[reference]->errorOccured == PR_FALSE)     \
    {                                                                   \
      PROCESS_PENDING_EVENTS_REF (reference);                                \
    }                                                                   \
  printf ("RECEIVE 3\n"); \
  if (factory->result_map[reference]->returnIdentifier == 0 || \
	  factory->result_map[reference]->errorOccured == PR_TRUE) \
  {  \
	  *name = NULL;                                                     \
  } else {                                                              \
  *name =                                                               \
    reinterpret_cast<cast>                                              \
    (factory->references.ReferenceObject (factory->result_map[reference]->returnIdentifier)); \
  } \
  printf ("RECEIVE_REFERENCE: %s result: %x = %d\n",                    \
          __func__, *name, factory->result_map[reference]->returnIdentifier);

// FIXME: track and free JNIIDs.
#define MESSAGE_RECEIVE_ID(reference, cast, id, signature)              \
  PRBool processed = PR_FALSE;                                          \
  nsresult res = NS_OK;                                                 \
  printf("RECEIVE ID 1\n");                                             \
  while (factory->result_map[reference]->returnIdentifier == -1 &&\
	     factory->result_map[reference]->errorOccured == PR_FALSE)     \
    {                                                                   \
      PROCESS_PENDING_EVENTS_REF (reference);                                \
    }                                                                   \
                                                                        \
  if (factory->result_map[reference]->errorOccured == PR_TRUE)	 	    \
  { \
	  *id = NULL; \
  } else \
  { \
  *id = reinterpret_cast<cast>                                  \
    (new JNIID (factory->result_map[reference]->returnIdentifier, signature));         \
   printf ("RECEIVE_ID: %s result: %x = %d, %s\n",               \
           __func__, *id, factory->result_map[reference]->returnIdentifier,             \
           signature); \
  }

#define MESSAGE_RECEIVE_VALUE(reference, ctype, result)                    \
  nsresult res = NS_OK;                                                    \
  printf("RECEIVE VALUE 1\n");                                             \
  while (factory->result_map[reference]->returnValue == "" && \
	     factory->result_map[reference]->errorOccured == PR_FALSE)            \
    {                                                                      \
      PROCESS_PENDING_EVENTS_REF (reference);                                   \
    }                                                                      \
    *result = ParseValue (type, factory->result_map[reference]->returnValue);            
// \
//   char* valueString = ValueString (type, *result);              \
//   printf ("RECEIVE_VALUE: %s result: %x = %s\n",                \
//           __func__, result, valueString);                       \
//   free (valueString);                                           \
//   valueString = NULL;

#define MESSAGE_RECEIVE_SIZE(reference, result)                   \
  PRBool processed = PR_FALSE;                                  \
  nsresult res = NS_OK;                                         \
  printf("RECEIVE SIZE 1\n");                                 \
  while (factory->result_map[reference]->returnValue == "" && \
	     factory->result_map[reference]->errorOccured == PR_FALSE) \
    {                                                           \
      PROCESS_PENDING_EVENTS_REF (reference);                        \
    }                                                           \
  nsresult conversionResult;                                    \
  if (factory->result_map[reference]->errorOccured == PR_TRUE) \
	*result = NULL; \
  else \
  { \
    *result = factory->result_map[reference]->returnValue.ToInteger (&conversionResult); \
    PLUGIN_CHECK ("parse integer", conversionResult);             \
  }
// \
//   printf ("RECEIVE_SIZE: %s result: %x = %d\n",                 \
//           __func__, result, *result);

// strdup'd string must be freed by calling function.
#define MESSAGE_RECEIVE_STRING(reference, char_type, result)      \
  PRBool processed = PR_FALSE;                                  \
  nsresult res = NS_OK;                                         \
  printf("RECEIVE STRING 1\n");                                 \
  while (factory->result_map[reference]->returnValue == "" && \
	     factory->result_map[reference]->errorOccured == PR_FALSE)  \
    {                                                           \
      PROCESS_PENDING_EVENTS_REF (reference);                        \
    }                                                           \
	if (factory->result_map[reference]->errorOccured == PR_TRUE) \
		*result = NULL; \
	else \
	{\
	  printf("Setting result to: %s\n", strdup (factory->result_map[reference]->returnValue.get ())); \
      *result = reinterpret_cast<char_type const*>                  \
                (strdup (factory->result_map[reference]->returnValue.get ()));\
	}
// \
//   printf ("RECEIVE_STRING: %s result: %x = %s\n",               \
//           __func__, result, *result);

// strdup'd string must be freed by calling function.
#define MESSAGE_RECEIVE_STRING_UCS(reference, result)             \
  PRBool processed = PR_FALSE;                                  \
  nsresult res = NS_OK;                                         \
  printf("RECEIVE STRING UCS 1\n");                                 \
  while (factory->result_map[reference]->returnValueUCS.IsEmpty() && \
	     factory->result_map[reference]->errorOccured == PR_FALSE) \
    {                                                           \
      PROCESS_PENDING_EVENTS_REF (reference);                        \
    }                                                           \
	if (factory->result_map[reference]->errorOccured == PR_TRUE) \
		*result = NULL; \
	else \
	{ \
	  int length = factory->result_map[reference]->returnValueUCS.Length ();               \
	  jchar* newstring = static_cast<jchar*> (PR_Malloc (length));  \
	  memset (newstring, 0, length);                                \
	  memcpy (newstring, factory->result_map[reference]->returnValueUCS.get (), length);   \
	  std::cout << "Setting result to: " << factory->result_map[reference]->returnValueUCS.get() << std::endl; \
	  *result = static_cast<jchar const*> (newstring); \
	}

// \
//   printf ("RECEIVE_STRING: %s result: %x = %s\n",               \
//           __func__, result, *result);

#define MESSAGE_RECEIVE_BOOLEAN(reference, result)                \
  PRBool processed = PR_FALSE;                                  \
  nsresult res = NS_OK;                                         \
  printf("RECEIVE BOOLEAN 1\n");                             \
  while (factory->result_map[reference]->returnIdentifier == -1 && \
	     factory->result_map[reference]->errorOccured == PR_FALSE)               \
    {                                                           \
      PROCESS_PENDING_EVENTS_REF (reference);                        \
    }                                                           \
	if (factory->result_map[reference]->errorOccured == PR_TRUE) \
		*result = NULL; \
	else \
	  *result = factory->result_map[reference]->returnIdentifier;
//      res = factory->current->ProcessNextEvent (PR_TRUE,        \
//                                                &processed);    \
//      PLUGIN_CHECK_RETURN (__func__, res);                      \

// \
//   printf ("RECEIVE_BOOLEAN: %s result: %x = %s\n",              \
//           __func__, result, *result ? "true" : "false");

#include <nscore.h>
#include <nsISupports.h>
#include <nsIFactory.h>

// Factory functions.
extern "C" NS_EXPORT nsresult NSGetFactory (nsISupports* aServMgr,
                                            nsCID const& aClass,
                                            char const* aClassName,
                                            char const* aContractID,
                                            nsIFactory** aFactory);



#include <nsIFactory.h>
#include <nsIPlugin.h>
#include <nsIJVMManager.h>
#include <nsIJVMPrefsWindow.h>
#include <nsIJVMPlugin.h>
#include <nsIInputStream.h>
#include <nsIAsyncInputStream.h>
#include <nsISocketTransport.h>
#include <nsIOutputStream.h>
#include <nsIAsyncInputStream.h>
#include <prthread.h>
#include <nsIThread.h>
#include <nsILocalFile.h>
#include <nsCOMPtr.h>
#include <nsIPluginInstance.h>
#include <nsIPluginInstancePeer.h>
#include <nsIJVMPluginInstance.h>
#include <nsIPluginTagInfo2.h>
#include <nsComponentManagerUtils.h>
#include <nsCOMPtr.h>
#include <nsILocalFile.h>
#include <prthread.h>
#include <queue>
#include <nsIEventTarget.h>
// // FIXME: I had to hack dist/include/xpcom/xpcom-config.h to comment
// // out this line: #define HAVE_CPP_2BYTE_WCHAR_T 1 so that
// // nsStringAPI.h would not trigger a compilation assertion failure:
// //
// // dist/include/xpcom/nsStringAPI.h:971: error: size of array ‘arg’
// // is negative
// #include <nsStringAPI.h>

// FIXME: if about:plugins doesn't show this plugin, try:
// export LD_LIBRARY_PATH=/home/fitzsim/sources/mozilla/xpcom/build

// FIXME: if the connection spins, printing:
//
// ICEDTEA PLUGIN: thread 0x84f4a68: wait for connection: process next event
// ICEDTEA PLUGIN: thread 0x84f4a68: Instance::IsConnected
// ICEDTEA PLUGIN: thread 0x84f4a68: Instance::IsConnected return
//
// repeatedly, it means there's a problem with pluginappletviewer.
// Try "make av".

#include <nsClassHashtable.h>
#include <nsDataHashtable.h>
#include <nsAutoPtr.h>

class IcedTeaPluginInstance;
class IcedTeaEventSink;

// TODO:
//
// 1) complete test suite for all used functions.
//
// 2) audit memory allocation/deallocation on C++ side
//
// 3) confirm objects/ids hashmap emptying on Java side

// IcedTeaPlugin cannot be a standard nsIScriptablePlugin.  It must
// conform to Mozilla's expectations since Mozilla treats Java as a
// special case.  See dom/src/base/nsDOMClassInfo.cpp:
// nsHTMLPluginObjElementSH::GetPluginJSObject.

// nsClassHashtable does JNIReference deallocation automatically,
// using nsAutoPtr.
class ReferenceHashtable
  : public nsClassHashtable<nsUint32HashKey, JNIReference>
{
public:
  jobject ReferenceObject (PRUint32);
  jobject ReferenceObject (PRUint32, char const*);
  void UnreferenceObject (PRUint32);
};

jobject
ReferenceHashtable::ReferenceObject (PRUint32 key)
{
  if (key == 0)
    return 0;

  JNIReference* reference;
  Get (key, &reference);
  if (reference == 0)
    {
      reference = new JNIReference (key);
      Put (key, reference);
    }
  reference->count++;
  printf ("INCREMENTED: %d %p to: %d\n", key, reference, reference->count);
  return reinterpret_cast<jobject> (reference);
}

jobject
ReferenceHashtable::ReferenceObject (PRUint32 key, char const* signature)
{
  if (key == 0)
    return 0;

  JNIReference* reference;
  Get (key, &reference);
  if (reference == 0)
    {
      reference = new JNIID (key, signature);
      Put (key, reference);
    }
  reference->count++;
  printf ("INCREMENTED: %d %p to: %d\n", key, reference, reference->count);
  return reinterpret_cast<jobject> (reference);
}

void
ReferenceHashtable::UnreferenceObject (PRUint32 key)
{
  JNIReference* reference;
  Get (key, &reference);
  if (reference != 0)
    {
      reference->count--;
      printf ("DECREMENTED: %d %p to: %d\n", key, reference, reference->count);
      if (reference->count == 0)
        Remove (key);
    }
}

class ResultContainer 
{
	public:
		ResultContainer();
		~ResultContainer();
		void Clear();
  		PRUint32 returnIdentifier;
		nsCString returnValue;
		nsString returnValueUCS;
		PRBool errorOccured;

};

ResultContainer::ResultContainer () 
{
	PLUGIN_TRACE_RC();

	returnIdentifier = -1;
	returnValue.Truncate();
	returnValueUCS.Truncate();
	errorOccured = PR_FALSE;
}

ResultContainer::~ResultContainer ()
{
	PLUGIN_TRACE_RC();

    returnIdentifier = -1;
	returnValue.Truncate();
	returnValueUCS.Truncate();
}

void
ResultContainer::Clear()
{
	PLUGIN_TRACE_RC();

	returnIdentifier = -1;
	returnValue.Truncate();
	returnValueUCS.Truncate();
	errorOccured = PR_FALSE;
}

#include <nsTArray.h>
#include <nsILiveconnect.h>
#include <nsIProcess.h>
#include <map>

class IcedTeaJNIEnv;

// nsIPlugin inherits from nsIFactory.
class IcedTeaPluginFactory : public nsIPlugin,
                             public nsIJVMManager,
                             public nsIJVMPrefsWindow,
                             public nsIJVMPlugin,
                             public nsIInputStreamCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY
  NS_DECL_NSIPLUGIN
  NS_DECL_NSIJVMMANAGER
  // nsIJVMPrefsWindow does not provide an NS_DECL macro.
public:
  NS_IMETHOD Show (void);
  NS_IMETHOD Hide (void);
  NS_IMETHOD IsVisible (PRBool* result);
  // nsIJVMPlugin does not provide an NS_DECL macro.
public:
  NS_IMETHOD AddToClassPath (char const* dirPath);
  NS_IMETHOD RemoveFromClassPath (char const* dirPath);
  NS_IMETHOD GetClassPath (char const** result);
  NS_IMETHOD GetJavaWrapper (JNIEnv* jenv, PLUGIN_JAVASCRIPT_TYPE obj, jobject* jobj);
  NS_IMETHOD CreateSecureEnv (JNIEnv* proxyEnv, nsISecureEnv** outSecureEnv);
  NS_IMETHOD SpendTime (PRUint32 timeMillis);
  NS_IMETHOD UnwrapJavaWrapper (JNIEnv* jenv, jobject jobj, PLUGIN_JAVASCRIPT_TYPE* obj);
  NS_DECL_NSIINPUTSTREAMCALLBACK

  IcedTeaPluginFactory();
  nsresult SendMessageToAppletViewer (nsCString& message);
  PRUint32 RegisterInstance (IcedTeaPluginInstance* instance);
  void UnregisterInstance (PRUint32 instance_identifier);
  NS_IMETHOD GetJavaObject (PRUint32 instance_identifier, jobject* object);
  void HandleMessage (nsCString const& message);
  nsresult SetTransport (nsISocketTransport* transport);
  void Connected ();
  void Disconnected ();
//  PRUint32 returnIdentifier;
//  nsCString returnValue;
//  nsString returnValueUCS;
  PRBool IsConnected ();
  nsCOMPtr<nsIAsyncInputStream> async;
  nsCOMPtr<nsIThread> current;
  nsCOMPtr<nsIInputStream> input;
  nsCOMPtr<nsIOutputStream> output;
  ReferenceHashtable references;
  PRBool shutting_down;
  // FIXME: make private?
  JNIEnv* proxyEnv;
  nsISecureEnv* secureEnv;
  std::map<PRUint32,ResultContainer*> result_map;
  void GetMember ();
  void SetMember ();
  void GetSlot ();
  void SetSlot ();
  void Eval ();
  void RemoveMember ();
  void Call ();
  void Finalize ();
  void ToString ();
  nsCOMPtr<nsILiveconnect> liveconnect;
  std::map<nsCString, nsCString> codebase_map;

private:
  ~IcedTeaPluginFactory();
  nsresult TestAppletviewer ();
  void DisplayFailureDialog ();
  nsresult StartAppletviewer ();
  void ProcessMessage();
  void ConsumeMsgFromJVM();
  void CreateSocket();
  nsCOMPtr<nsIThread> processThread;
  nsCOMPtr<IcedTeaEventSink> sink;
  nsCOMPtr<nsISocketTransport> transport;
  nsCOMPtr<nsIProcess> applet_viewer_process;
  PRBool connected;
  PRUint32 next_instance_identifier;
  // Does not do construction/deconstruction or reference counting.
  nsDataHashtable<nsUint32HashKey, IcedTeaPluginInstance*> instances;
  PRUint32 object_identifier_return;
  PRMonitor *jvmMsgQueuePRMonitor;
  std::queue<nsCString> jvmMsgQueue;
  int javascript_identifier;
  int name_identifier;
  int args_identifier;
  int string_identifier;
  int slot_index;
  int value_identifier;

/**
 * JNI I/O related code
 *
 
  void WriteToJVM(nsCString& message);
  void InitJVM();
  void ReadFromJVM();

  PRMonitor *jvmPRMonitor;
  nsCOMPtr<nsIThread> readThread;
  JavaVM *jvm;
  JNIEnv *javaEnv;
  jclass javaPluginClass;
  jobject javaPluginObj;
  jmethodID getMessageMID;
  jmethodID postMessageMID;

  */

};

class IcedTeaEventSink;

class IcedTeaPluginInstance : public nsIPluginInstance,
                              public nsIJVMPluginInstance
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPLUGININSTANCE
  NS_DECL_NSIJVMPLUGININSTANCE

  IcedTeaPluginInstance (IcedTeaPluginFactory* factory);
  ~IcedTeaPluginInstance ();

  void GetWindow ();

  nsIPluginInstancePeer* peer;
  PRBool initialized;

private:

  PLUGIN_JAVASCRIPT_TYPE liveconnect_window;
  gpointer window_handle;
  guint32 window_width;
  guint32 window_height;
  // FIXME: nsCOMPtr.
  IcedTeaPluginFactory* factory;
  PRUint32 instance_identifier;
  nsCString instanceIdentifierPrefix;
};


#include <nsISocketProviderService.h>
#include <nsISocketProvider.h>
#include <nsIServerSocket.h>
#include <nsIComponentManager.h>
#include <nsIPluginInstance.h>
#include <sys/types.h>
#include <sys/stat.h>

class IcedTeaSocketListener : public nsIServerSocketListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER

  IcedTeaSocketListener (IcedTeaPluginFactory* factory);

private:
  ~IcedTeaSocketListener ();
  IcedTeaPluginFactory* factory;
protected:
};

#include <nsITransport.h>

class IcedTeaEventSink : public nsITransportEventSink
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITRANSPORTEVENTSINK

  IcedTeaEventSink();

private:
  ~IcedTeaEventSink();
};

#include <nsISupports.h>
#include <nsISecureEnv.h>
#include <nsISecurityContext.h>
#include <nsIServerSocket.h>
#include <nsNetCID.h>
#include <nsThreadUtils.h>
#include <nsIThreadManager.h>
#include <nsIThread.h>
#include <nsXPCOMCIDInternal.h>

class IcedTeaJNIEnv : public nsISecureEnv
{
  NS_DECL_ISUPPORTS

  // nsISecureEnv does not provide an NS_DECL macro.
public:
  IcedTeaJNIEnv (IcedTeaPluginFactory* factory);

  NS_IMETHOD NewObject (jclass clazz,
                        jmethodID methodID,
                        jvalue* args,
                        jobject* result,
                        nsISecurityContext* ctx = NULL);

  NS_IMETHOD CallMethod (jni_type type,
                         jobject obj,
                         jmethodID methodID,
                         jvalue* args,
                         jvalue* result,
                         nsISecurityContext* ctx = NULL);

  NS_IMETHOD CallNonvirtualMethod (jni_type type,
                                   jobject obj,
                                   jclass clazz,
                                   jmethodID methodID,
                                   jvalue* args,
                                   jvalue* result,
                                   nsISecurityContext* ctx = NULL);

  NS_IMETHOD GetField (jni_type type,
                       jobject obj,
                       jfieldID fieldID,
                       jvalue* result,
                       nsISecurityContext* ctx = NULL);

  NS_IMETHOD SetField (jni_type type,
                       jobject obj,
                       jfieldID fieldID,
                       jvalue val,
                       nsISecurityContext* ctx = NULL);

  NS_IMETHOD CallStaticMethod (jni_type type,
                               jclass clazz,
                               jmethodID methodID,
                               jvalue* args,
                               jvalue* result,
                               nsISecurityContext* ctx = NULL);

  NS_IMETHOD GetStaticField (jni_type type,
                             jclass clazz,
                             jfieldID fieldID,
                             jvalue* result,
                             nsISecurityContext* ctx = NULL);


  NS_IMETHOD SetStaticField (jni_type type,
                             jclass clazz,
                             jfieldID fieldID,
                             jvalue val,
                             nsISecurityContext* ctx = NULL);


  NS_IMETHOD GetVersion (jint* version);

  NS_IMETHOD DefineClass (char const* name,
                          jobject loader,
                          jbyte const* buf,
                          jsize len,
                          jclass* clazz);

  NS_IMETHOD FindClass (char const* name,
                        jclass* clazz);

  NS_IMETHOD GetSuperclass (jclass sub,
                            jclass* super);

  NS_IMETHOD IsAssignableFrom (jclass sub,
                               jclass super,
                               jboolean* result);

  NS_IMETHOD Throw (jthrowable obj,
                    jint* result);

  NS_IMETHOD ThrowNew (jclass clazz,
                       char const* msg,
                       jint* result);

  NS_IMETHOD ExceptionOccurred (jthrowable* result);

  NS_IMETHOD ExceptionDescribe (void);

  NS_IMETHOD ExceptionClear (void);

  NS_IMETHOD FatalError (char const* msg);

  NS_IMETHOD NewGlobalRef (jobject lobj,
                           jobject* result);

  NS_IMETHOD DeleteGlobalRef (jobject gref);

  NS_IMETHOD DeleteLocalRef (jobject obj);

  NS_IMETHOD IsSameObject (jobject obj1,
                           jobject obj2,
                           jboolean* result);

  NS_IMETHOD AllocObject (jclass clazz,
                          jobject* result);

  NS_IMETHOD GetObjectClass (jobject obj,
                             jclass* result);

  NS_IMETHOD IsInstanceOf (jobject obj,
                           jclass clazz,
                           jboolean* result);

  NS_IMETHOD GetMethodID (jclass clazz,
                          char const* name,
                          char const* sig,
                          jmethodID* id);

  NS_IMETHOD GetFieldID (jclass clazz,
                         char const* name,
                         char const* sig,
                         jfieldID* id);

  NS_IMETHOD GetStaticMethodID (jclass clazz,
                                char const* name,
                                char const* sig,
                                jmethodID* id);

  NS_IMETHOD GetStaticFieldID (jclass clazz,
                               char const* name,
                               char const* sig,
                               jfieldID* id);

  NS_IMETHOD NewString (jchar const* unicode,
                        jsize len,
                        jstring* result);

  NS_IMETHOD GetStringLength (jstring str,
                              jsize* result);

  NS_IMETHOD GetStringChars (jstring str,
                             jboolean* isCopy,
                             jchar const** result);

  NS_IMETHOD ReleaseStringChars (jstring str,
                                 jchar const* chars);

  NS_IMETHOD NewStringUTF (char const* utf,
                           jstring* result);

  NS_IMETHOD GetStringUTFLength (jstring str,
                                 jsize* result);

  NS_IMETHOD GetStringUTFChars (jstring str,
                                jboolean* isCopy,
                                char const** result);

  NS_IMETHOD ReleaseStringUTFChars (jstring str,
                                    char const* chars);

  NS_IMETHOD GetArrayLength (jarray array,
                             jsize* result);

  NS_IMETHOD NewObjectArray (jsize len,
                             jclass clazz,
                             jobject init,
                             jobjectArray* result);

  NS_IMETHOD GetObjectArrayElement (jobjectArray array,
                                    jsize index,
                                    jobject* result);

  NS_IMETHOD SetObjectArrayElement (jobjectArray array,
                                    jsize index,
                                    jobject val);

  NS_IMETHOD NewArray (jni_type element_type,
                       jsize len,
                       jarray* result);

  NS_IMETHOD GetArrayElements (jni_type type,
                               jarray array,
                               jboolean* isCopy,
                               void* result);

  NS_IMETHOD ReleaseArrayElements (jni_type type,
                                   jarray array,
                                   void* elems,
                                   jint mode);

  NS_IMETHOD GetArrayRegion (jni_type type,
                             jarray array,
                             jsize start,
                             jsize len,
                             void* buf);

  NS_IMETHOD SetArrayRegion (jni_type type,
                             jarray array,
                             jsize start,
                             jsize len,
                             void* buf);

  NS_IMETHOD RegisterNatives (jclass clazz,
                              JNINativeMethod const* methods,
                              jint nMethods,
                              jint* result);

  NS_IMETHOD UnregisterNatives (jclass clazz,
                                jint* result);

  NS_IMETHOD MonitorEnter (jobject obj,
                           jint* result);

  NS_IMETHOD MonitorExit (jobject obj,
                          jint* result);

  NS_IMETHOD GetJavaVM (JavaVM** vm,
                        jint* result);

  jvalue ParseValue (jni_type type, nsCString& str);
  char* ExpandArgs (JNIID* id, jvalue* args);
  char* ValueString (jni_type type, jvalue value);

private:
  ~IcedTeaJNIEnv ();

  IcedTeaPluginFactory* factory;

  PRMonitor *contextCounterPRMonitor;

  int IncrementContextCounter();
  void DecrementContextCounter();
  nsresult GetCurrentContextAddr(char *addr);
  nsresult GetCurrentPageAddress(const char **addr);
  int contextCounter;
};


#include <nsIServerSocket.h>
#include <nsNetError.h>
#include <nsPIPluginInstancePeer.h>
#include <nsIPluginInstanceOwner.h>
#include <nsIRunnable.h>
#include <iostream>

class IcedTeaRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  IcedTeaRunnable ();

  ~IcedTeaRunnable ();
};

NS_IMPL_ISUPPORTS1 (IcedTeaRunnable, nsIRunnable)

IcedTeaRunnable::IcedTeaRunnable ()
{
}

IcedTeaRunnable::~IcedTeaRunnable ()
{
}

NS_IMETHODIMP
IcedTeaRunnable::Run ()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

template <class T>
class IcedTeaRunnableMethod : public IcedTeaRunnable
{
public:
  typedef void (T::*Method) ();

  IcedTeaRunnableMethod (T* object, Method method);
  NS_IMETHOD Run ();

  ~IcedTeaRunnableMethod ();

  T* object;
  Method method;
};

template <class T>
IcedTeaRunnableMethod<T>::IcedTeaRunnableMethod (T* object, Method method)
: object (object),
  method (method)
{
  NS_ADDREF (object);
}

template <class T>
IcedTeaRunnableMethod<T>::~IcedTeaRunnableMethod ()
{
  NS_RELEASE (object);
}

template <class T> NS_IMETHODIMP
IcedTeaRunnableMethod<T>::Run ()
{
    (object->*method) ();
    return NS_OK;
}


// FIXME: Special class just for dispatching GetURL to another 
// thread.. seriously, a class just for that? there has to be a better way!

class GetURLRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE

  GetURLRunnable (nsIPluginInstancePeer* peer, const char* url, const char* target);

  ~GetURLRunnable ();

private:
  nsIPluginInstancePeer* peer;
  const char* url;
  const char* target;
};

NS_IMPL_ISUPPORTS1 (GetURLRunnable, nsIRunnable)

GetURLRunnable::GetURLRunnable (nsIPluginInstancePeer* peer, const char* url, const char* target)
: peer(peer),
  url(url),
  target(target)
{
    NS_ADDREF (peer);
}

GetURLRunnable::~GetURLRunnable ()
{
  NS_RELEASE(peer);
}

NS_IMETHODIMP
GetURLRunnable::Run ()
{
   nsCOMPtr<nsPIPluginInstancePeer> ownerGetter =
                do_QueryInterface (peer);
   nsIPluginInstanceOwner* owner = nsnull;
   ownerGetter->GetOwner (&owner);

   return owner->GetURL ((const char*) url, (const char*) target,
                         nsnull, 0, nsnull, 0);
}


NS_IMPL_ISUPPORTS6 (IcedTeaPluginFactory, nsIFactory, nsIPlugin, nsIJVMManager,
                    nsIJVMPrefsWindow, nsIJVMPlugin, nsIInputStreamCallback)

// IcedTeaPluginFactory functions.
IcedTeaPluginFactory::IcedTeaPluginFactory ()
: next_instance_identifier (1),
  proxyEnv (0),
  javascript_identifier (0),
  name_identifier (0),
  args_identifier (0),
  string_identifier (0),
  slot_index (0),
  value_identifier (0),
  connected (PR_FALSE),
  liveconnect (0),
  shutting_down(PR_FALSE)
{
  PLUGIN_TRACE_FACTORY ();
  instances.Init ();
  references.Init ();
  printf ("CONSTRUCTING FACTORY\n");
}

IcedTeaPluginFactory::~IcedTeaPluginFactory ()
{
  // FIXME: why did this crash with threadManager == 0x0 on shutdown?
  PLUGIN_TRACE_FACTORY ();
  secureEnv = 0;
  factory_created = PR_FALSE;
  factory = NULL;
  printf ("DECONSTRUCTING FACTORY\n");
}

// nsIFactory functions.
NS_IMETHODIMP
IcedTeaPluginFactory::CreateInstance (nsISupports* aOuter, nsIID const& iid,
                                      void** result)
{
  PLUGIN_TRACE_FACTORY ();
  if (!result)
    return NS_ERROR_NULL_POINTER;

  *result = NULL;

  IcedTeaPluginInstance* instance = new IcedTeaPluginInstance (this);

  if (!instance)
    return NS_ERROR_OUT_OF_MEMORY;

  return instance->QueryInterface (iid, result);
}

NS_IMETHODIMP
IcedTeaPluginFactory::LockFactory (PRBool lock)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIPlugin functions.
NS_IMETHODIMP
IcedTeaPluginFactory::CreatePluginInstance (nsISupports* aOuter,
                                            nsIID const& aIID,
                                            char const* aPluginMIMEType,
                                            void** aResult)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::Initialize ()
{
  PLUGIN_TRACE_FACTORY ();
  nsresult result = NS_OK;

  PLUGIN_DEBUG_TWO ("Factory::Initialize: using", appletviewer_executable);

  nsCOMPtr<nsIComponentManager> manager;
  result = NS_GetComponentManager (getter_AddRefs (manager));

/**
 * JNI I/O code
 
  // Initialize mutex to control access to the jvm
  jvmPRMonitor = PR_NewMonitor();
*/
  jvmMsgQueuePRMonitor = PR_NewMonitor();

  nsCOMPtr<nsIThreadManager> threadManager;
  result = manager->CreateInstanceByContractID
    (NS_THREADMANAGER_CONTRACTID, nsnull, NS_GET_IID (nsIThreadManager),
     getter_AddRefs (threadManager));
  PLUGIN_CHECK_RETURN ("thread manager", result);

  result = threadManager->GetCurrentThread (getter_AddRefs (current));
  PLUGIN_CHECK_RETURN ("current thread", result);

/*
 *
 * Socket related code for TCP/IP communication
 *
 */
 
  nsCOMPtr<nsIThread> socketCreationThread;
  nsCOMPtr<nsIRunnable> socketCreationEvent =
							new IcedTeaRunnableMethod<IcedTeaPluginFactory>
							(this, &IcedTeaPluginFactory::IcedTeaPluginFactory::CreateSocket);

  NS_NewThread(getter_AddRefs(socketCreationThread), socketCreationEvent);

  PLUGIN_DEBUG ("Instance::Initialize: awaiting connection from appletviewer");
  PRBool processed;

  // FIXME: move this somewhere applet-window specific so it doesn't block page
  // display.
  // FIXME: this doesn't work with thisiscool.com.
  while (!IsConnected ())
    {
//      result = socketCreationThread->ProcessNextEvent (PR_TRUE, &processed);
//      PLUGIN_CHECK_RETURN ("wait for connection: process next event", result);
    }
  PLUGIN_DEBUG ("Instance::Initialize:"
                " got confirmation that appletviewer is running.");

  return NS_OK;
}

void
IcedTeaPluginFactory::CreateSocket ()
{

  PRBool processed;
  nsresult result;

  // Start appletviewer process for this plugin instance.
  nsCOMPtr<nsIComponentManager> manager;
  result = NS_GetComponentManager (getter_AddRefs (manager));
  PLUGIN_CHECK ("get component manager", result);

  result = manager->CreateInstance
    (nsILiveconnect::GetCID (),
     nsnull, NS_GET_IID (nsILiveconnect),
     getter_AddRefs (liveconnect));
  PLUGIN_CHECK ("liveconnect", result);

  nsCOMPtr<nsIThreadManager> threadManager;
  nsCOMPtr<nsIThread> curr_thread;
  result = manager->CreateInstanceByContractID
    (NS_THREADMANAGER_CONTRACTID, nsnull, NS_GET_IID (nsIThreadManager),
     getter_AddRefs (threadManager));
  PLUGIN_CHECK ("thread manager", result);

  result = threadManager->GetCurrentThread (getter_AddRefs (curr_thread));


/*
 * Socket initialization code for TCP/IP communication
 *
 */
 
  nsCOMPtr<nsIServerSocket> socket;
  result = manager->CreateInstanceByContractID (NS_SERVERSOCKET_CONTRACTID,
                                                nsnull,
                                                NS_GET_IID (nsIServerSocket),
                                                getter_AddRefs (socket));
  PLUGIN_CHECK ("create server socket", result);

  // FIXME: hard-coded port
  result = socket->Init (50007, PR_TRUE, -1);


  PLUGIN_CHECK ("socket init", result);

  nsCOMPtr<IcedTeaSocketListener> listener = new IcedTeaSocketListener (this);
  result = socket->AsyncListen (listener);
  PLUGIN_CHECK ("add socket listener", result);

  result = StartAppletviewer ();
  PLUGIN_CHECK ("started appletviewer", result);

  while (!IsConnected()) 
  {
      result = curr_thread->ProcessNextEvent (PR_TRUE, &processed);
      PLUGIN_CHECK ("wait for connection: process next event", result);
  }

}

NS_IMETHODIMP
IcedTeaPluginFactory::Shutdown ()
{
  shutting_down = PR_TRUE;

  nsCString shutdownStr("shutdown");
  SendMessageToAppletViewer(shutdownStr);

  // wake up process thread to tell it to shutdown
  PRThread *prThread;
  processThread->GetPRThread(&prThread);
  printf("Interrupting process thread...");
  PRStatus res = PR_Interrupt(prThread);
  printf(" done!\n");

  PRInt32 exitVal;
  applet_viewer_process->GetExitValue(&exitVal);

/*
  PRUint32 max_sleep_time = 2000;
  PRUint32 sleep_time = 0;
  while ((sleep_time < max_sleep_time) && (exitVal == -1)) {
	  printf("Appletviewer still appears to be running. Waiting...\n");
	  PR_Sleep(200);
	  sleep_time += 200;
	  applet_viewer_process->GetExitValue(&exitVal);
  }

  // still running? kill it with extreme prejudice
  applet_viewer_process->GetExitValue(&exitVal);
  if (exitVal == -1) {
	  printf("Appletviewer still appears to be running. Trying to kill it...\n");
	  applet_viewer_process->Kill();
  }
*/

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginFactory::GetMIMEDescription (char const** aMIMEDescription)
{
  PLUGIN_TRACE_FACTORY ();
  *aMIMEDescription = PLUGIN_MIME_DESC;

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginFactory::GetValue (nsPluginVariable aVariable, void* aValue)
{
  PLUGIN_TRACE_FACTORY ();
  nsresult result = NS_OK;

  switch (aVariable)
    {
    case nsPluginVariable_NameString:
      *static_cast<char const**> (aValue) = PLUGIN_NAME;
      break;
    case nsPluginVariable_DescriptionString:
      *static_cast<char const**> (aValue) = PLUGIN_DESCRIPTION;
      break;
    default:
      PLUGIN_ERROR ("Unknown plugin value requested.");
      result = NS_ERROR_INVALID_ARG;
      break;
    }

  return result;
}

// nsIJVMManager functions.
NS_IMETHODIMP
IcedTeaPluginFactory::CreateProxyJNI (nsISecureEnv* secureEnv,
                                      JNIEnv** outProxyEnv)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::GetProxyJNI (JNIEnv** outProxyEnv)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::ShowJavaConsole ()
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::IsAllPermissionGranted (char const* lastFingerprint,
                                              char const* lastCommonName,
                                              char const* rootFingerprint,
                                              char const* rootCommonName,
                                              PRBool* _retval)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::IsAppletTrusted (char const* aRSABuf, PRUint32 aRSABufLen,
                                       char const* aPlaintext,
                                       PRUint32 aPlaintextLen,
                                       PRBool* isTrusted,
                                       nsIPrincipal**_retval)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::GetJavaEnabled (PRBool* aJavaEnabled)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIJVMPrefsWindow functions.
NS_IMETHODIMP
IcedTeaPluginFactory::Show (void)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::Hide (void)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::IsVisible(PRBool* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::AddToClassPath (char const* dirPath)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::RemoveFromClassPath (char const* dirPath)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::GetClassPath (char const** result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::GetJavaWrapper (JNIEnv* jenv, PLUGIN_JAVASCRIPT_TYPE obj,
                                      jobject* jobj)
{
  jclass clazz;
  jmethodID method;
  jobject newobject;
  jvalue args[1];
  secureEnv->FindClass ("netscape.javascript.JSObject", &clazz);
  secureEnv->GetMethodID (clazz, "<init>",
                          PLUGIN_JAVASCRIPT_SIGNATURE, &method);
  PLUGIN_INITIALIZE_JAVASCRIPT_ARGUMENT(args, obj);
  secureEnv->NewObject (clazz, method, args, &newobject);
  *jobj = newobject;
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginFactory::CreateSecureEnv (JNIEnv* proxyEnv,
                                       nsISecureEnv** outSecureEnv)
{
  PLUGIN_TRACE_FACTORY ();
  *outSecureEnv = new IcedTeaJNIEnv (this);
  secureEnv = *outSecureEnv;
  IcedTeaPluginFactory::proxyEnv = proxyEnv;

  jclass clazz;
  jclass stringclazz;
  jmethodID method;
  jfieldID field;
  jvalue result;
  jvalue val;
  jvalue args[1];
  jarray array;
  jobjectArray objectarray;
  jobject newobject;
  jobject newglobalobject;
  jsize length;
  jboolean isCopy;
  char const* str;
  jchar const* jstr;
  jclass superclazz;
  jclass resultclazz;
  jboolean resultbool;

  printf ("CREATESECUREENV\n");
#if 0

  // IcedTeaJNIEnv::AllocObject
  // IcedTeaJNIEnv::CallMethod
  // IcedTeaJNIEnv::CallNonvirtualMethod
  // IcedTeaJNIEnv::CallStaticMethod
  // IcedTeaJNIEnv::DefineClass
  // IcedTeaJNIEnv::DeleteGlobalRef
  // IcedTeaJNIEnv::DeleteLocalRef
  // IcedTeaJNIEnv::ExceptionClear
  // IcedTeaJNIEnv::ExceptionDescribe
  // IcedTeaJNIEnv::ExceptionOccurred
  // IcedTeaJNIEnv::ExpandArgs
  // IcedTeaJNIEnv::FatalError
  // IcedTeaJNIEnv::FindClass
  // IcedTeaJNIEnv::GetArrayElements
  // IcedTeaJNIEnv::GetArrayLength
  // IcedTeaJNIEnv::GetArrayRegion
  // IcedTeaJNIEnv::GetFieldID
  // IcedTeaJNIEnv::GetField
  // IcedTeaJNIEnv::GetJavaVM
  // IcedTeaJNIEnv::GetMethodID
  // IcedTeaJNIEnv::GetObjectArrayElement
  // IcedTeaJNIEnv::GetObjectClass
  // IcedTeaJNIEnv::GetStaticFieldID
  // IcedTeaJNIEnv::GetStaticField
  // IcedTeaJNIEnv::GetStaticMethodID
  // IcedTeaJNIEnv::GetStringChars
  // IcedTeaJNIEnv::GetStringLength
  // IcedTeaJNIEnv::GetStringUTFChars
  // IcedTeaJNIEnv::GetStringUTFLength
  // IcedTeaJNIEnv::GetSuperclass
  // IcedTeaJNIEnv::GetVersion
  // IcedTeaJNIEnv::~IcedTeaJNIEnv
  // IcedTeaJNIEnv::IcedTeaJNIEnv
  // IcedTeaJNIEnv::IsAssignableFrom
  // IcedTeaJNIEnv::IsInstanceOf
  // IcedTeaJNIEnv::IsSameObject
  // IcedTeaJNIEnv::MonitorEnter
  // IcedTeaJNIEnv::MonitorExit
  // IcedTeaJNIEnv::NewArray
  // IcedTeaJNIEnv::NewGlobalRef
  // IcedTeaJNIEnv::NewObjectArray
  // IcedTeaJNIEnv::NewObject
  // IcedTeaJNIEnv::NewString
  // IcedTeaJNIEnv::NewStringUTF
  // IcedTeaJNIEnv::ParseValue
  // IcedTeaJNIEnv::RegisterNatives
  // IcedTeaJNIEnv::ReleaseArrayElements
  // IcedTeaJNIEnv::ReleaseStringChars
  // IcedTeaJNIEnv::ReleaseStringUTFChars
  // IcedTeaJNIEnv::SetArrayRegion
  // IcedTeaJNIEnv::SetField
  // IcedTeaJNIEnv::SetObjectArrayElement
  // IcedTeaJNIEnv::SetStaticField
  // IcedTeaJNIEnv::Throw
  // IcedTeaJNIEnv::ThrowNew
  // IcedTeaJNIEnv::UnregisterNatives
  // IcedTeaJNIEnv::ValueString

  (*outSecureEnv)->FindClass ("sun.applet.TestEnv", &clazz);
  (*outSecureEnv)->GetStaticMethodID (clazz, "TestIt", "()V", &method);
  result.i = -1;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, NULL, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (void)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItBool", "(Z)V", &method);
  args[0].z = 1;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (bool)");
  args[0].z = 0;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (bool)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItByte", "(B)V", &method);
  args[0].b = 0x35;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (byte)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItChar", "(C)V", &method);
  args[0].c = 'a';
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (char)");
  args[0].c = 'T';
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (char)");
  args[0].c = static_cast<jchar> (0x6C34);
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (char)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItShort", "(S)V", &method);
  args[0].s = 254;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (short)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItInt", "(I)V", &method);
  args[0].i = 68477325;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (int)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItLong", "(J)V", &method);
  args[0].j = 268435455;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (long)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItFloat", "(F)V", &method);
  args[0].f = 2.6843;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (float)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItDouble", "(D)V", &method);
  args[0].d = 3.6843E32;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (double)");

  (*outSecureEnv)->GetMethodID (clazz, "<init>", "()V", &method);
  printf ("HERE1\n");
  (*outSecureEnv)->NewObject (clazz, method, NULL, &newobject);
  printf ("HERE2\n");
  (*outSecureEnv)->IsSameObject (newobject, newobject, &resultbool);
  PLUGIN_TEST (resultbool, "IsSameObject: obj, obj");
  (*outSecureEnv)->IsSameObject (newobject, NULL, &resultbool);
  PLUGIN_TEST (!resultbool, "IsSameObject: obj, NULL");
  (*outSecureEnv)->IsSameObject (NULL, newobject, &resultbool);
  PLUGIN_TEST (!resultbool, "IsSameObject: NULL, obj");
  (*outSecureEnv)->IsSameObject (NULL, NULL, &resultbool);
  PLUGIN_TEST (resultbool, "IsSameObject: NULL, NULL");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObject",
                                      "(Lsun/applet/TestEnv;)V", &method);
  args[0].l = newobject;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (object)");

  
  printf ("HERE3\n");
  (*outSecureEnv)->NewGlobalRef (newobject, &newglobalobject);
  printf ("HERE4\n");
  (*outSecureEnv)->DeleteLocalRef (newobject);
  printf ("HERE5\n");
  (*outSecureEnv)->DeleteGlobalRef (newglobalobject);
  printf ("HERE6\n");

  (*outSecureEnv)->NewArray (jint_type, 10, &array);
  (*outSecureEnv)->GetArrayLength (array, &length);
  PLUGIN_TEST (length == 10, "GetArrayLength");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItIntArray", "([I)V",
                                      &method);
  args[0].l = array;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (int_array)");

  (*outSecureEnv)->FindClass ("java.lang.String", &stringclazz);
  (*outSecureEnv)->NewObjectArray (10, stringclazz, NULL, &objectarray);
  (*outSecureEnv)->GetMethodID (stringclazz, "<init>", "()V", &method);
  (*outSecureEnv)->NewObject (stringclazz, method, NULL, &newobject);
  (*outSecureEnv)->SetObjectArrayElement (objectarray, 3, newobject);

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectArray",
                                      "([Ljava/lang/String;)V", &method);
  args[0].l = objectarray;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (result.i == 0, "CallStaticMethod: static void (object_array)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItBoolReturnTrue", "()Z",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jboolean_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.z == JNI_TRUE, "CallStaticMethod: static bool (void)");
  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItBoolReturnFalse", "()Z",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jboolean_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.z == JNI_FALSE, "CallStaticMethod: static bool (void)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItByteReturn", "()B",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jbyte_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.b == static_cast<jbyte> (0xfe),
               "CallStaticMethod: static byte (void)");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItCharReturn", "()C",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jchar_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.c == 'K', "CHAR STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItCharUnicodeReturn", "()C",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jchar_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.c == static_cast<jchar> (0x6C34), "char static void: 0x6c34");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItShortReturn", "()S",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jshort_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.s == static_cast<jshort> (23), "SHORT STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItIntReturn", "()I",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jint_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.i == 3445, "INT STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItLongReturn", "()J",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jlong_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.j == 3242883, "LONG STATIC VOID");
  

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItFloatReturn", "()F",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jfloat_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.f == 9.21E4f, "FLOAT STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItDoubleReturn", "()D",
                                      &method);
  (*outSecureEnv)->CallStaticMethod (jdouble_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (result.d == 8.33E88, "DOUBLE STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectReturn",
                                      "()Ljava/lang/Object;", &method);
  (*outSecureEnv)->CallStaticMethod (jobject_type, clazz, method, NULL,
                                     &result);

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectString",
                                      "(Ljava/lang/String;)V", &method);
  args[0].l = result.l;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  // FIXME:
  PLUGIN_TEST (1, "RETURNED OBJECT");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItIntArrayReturn", "()[I",
                                      &method);
  printf ("GOT METHOD: %d\n", reinterpret_cast<JNIID*> (method)->identifier);
  (*outSecureEnv)->CallStaticMethod (jobject_type, clazz, method, NULL,
                                     &result);
  // FIXME:
  PLUGIN_TEST (1, "INT ARRAY STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItIntArray", "([I)V",
                                      &method);
  args[0].l = result.l;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (1, "RETURNED INT ARRAY");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectArrayReturn",
                                      "()[Ljava/lang/String;", &method);
  (*outSecureEnv)->CallStaticMethod (jobject_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (1, "OBJECT ARRAY STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectArray",
                                      "([Ljava/lang/String;)V", &method);
  args[0].l = result.l;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (1, "RETURNED OBJECT ARRAY");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectArrayMultiReturn",
                                      "()[[Ljava/lang/String;", &method);
  (*outSecureEnv)->CallStaticMethod (jobject_type, clazz, method, NULL,
                                     &result);
  PLUGIN_TEST (1, "OBJECT MULTIDIMENTIONAL ARRAY STATIC VOID");

  (*outSecureEnv)->GetStaticMethodID (clazz, "TestItObjectArrayMulti",
                                      "([[Ljava/lang/String;)V", &method);
  args[0].l = result.l;
  (*outSecureEnv)->CallStaticMethod (jvoid_type, clazz, method, args, &result);
  PLUGIN_TEST (1, "RETURNED OBJECT MULTIDIMENTIONAL ARRAY");

  (*outSecureEnv)->GetStaticFieldID (clazz, "intField", "I",
                                     &field);
  (*outSecureEnv)->GetStaticField (jint_type, clazz, field, &result);
  val.i = 788;
  (*outSecureEnv)->SetStaticField (jint_type, clazz, field, val);
  (*outSecureEnv)->GetStaticField (jint_type, clazz, field, &result);
  PLUGIN_TEST (1, "STATIC INT");
  PLUGIN_TEST (result.i == 788, "OBJECT STATIC VOID");

  (*outSecureEnv)->GetMethodID (clazz, "<init>", "()V", &method);
  (*outSecureEnv)->NewObject (clazz, method, NULL, &newobject);
  (*outSecureEnv)->GetMethodID (clazz, "TestItIntInstance", "(I)I", &method);
  args[0].i = 6322;
  (*outSecureEnv)->CallMethod (jint_type, newobject, method, args, &result);
  PLUGIN_TEST (result.i == 899, "NEW OBJECT");

  (*outSecureEnv)->GetFieldID (clazz, "intInstanceField", "I", &field);
  (*outSecureEnv)->GetField (jint_type, newobject, field, &result);
  val.i = 3224;
  (*outSecureEnv)->SetField (jint_type, newobject, field, val);
  (*outSecureEnv)->GetField (jint_type, newobject, field, &result);
  PLUGIN_TEST (result.i == 3224, "int field: 3224");

  (*outSecureEnv)->GetFieldID (clazz, "stringField",
                               "Ljava/lang/String;", &field);
  (*outSecureEnv)->GetField (jobject_type, newobject, field, &result);
  (*outSecureEnv)->GetStringUTFLength (
    reinterpret_cast<jstring> (result.l), &length);
  PLUGIN_TEST (length == 5, "UTF-8 STRING");
  (*outSecureEnv)->GetStringUTFChars (
    reinterpret_cast<jstring> (result.l), &isCopy, &str);
  PLUGIN_TEST (!strcmp (str, "hello"), "HI");

  (*outSecureEnv)->GetFieldID (clazz, "complexStringField",
                               "Ljava/lang/String;", &field);
  (*outSecureEnv)->GetField (jobject_type, newobject, field, &result);
  (*outSecureEnv)->GetStringUTFLength (
    reinterpret_cast<jstring> (result.l), &length);
  PLUGIN_TEST (length == 4, "HI");
  (*outSecureEnv)->GetStringUTFChars (
    reinterpret_cast<jstring> (result.l), &isCopy, &str);
  char expected1[8] = { 0x7A, 0xF0, 0x9D, 0x84, 0x9E, 0xE6, 0xB0, 0xB4 };
  PLUGIN_TEST (!memcmp (str, expected1, 8),
               "string field: 0x7A, 0xF0, 0x9D, 0x84, 0x9E, 0xE6, 0xB0, 0xB4");

  (*outSecureEnv)->GetFieldID (clazz, "stringField",
                               "Ljava/lang/String;", &field);
  (*outSecureEnv)->GetField (jobject_type, newobject, field, &result);
  (*outSecureEnv)->GetStringLength (
    reinterpret_cast<jstring> (result.l), &length);
  PLUGIN_TEST (length == 5, "UTF-16 STRING");
  (*outSecureEnv)->GetStringChars (
    reinterpret_cast<jstring> (result.l), &isCopy, &jstr);
  char expected2[10] = { 'h', 0x0, 'e', 0x0, 'l', 0x0, 'l', 0x0, 'o', 0x0 };
  PLUGIN_TEST (!memcmp (jstr, expected2, 10), "string field: hello");

  (*outSecureEnv)->GetFieldID (clazz, "complexStringField",
                               "Ljava/lang/String;", &field);
  (*outSecureEnv)->GetField (jobject_type, newobject, field, &result);
  (*outSecureEnv)->GetStringLength (
    reinterpret_cast<jstring> (result.l), &length);
  PLUGIN_TEST (length == 4, "HI");
  (*outSecureEnv)->GetStringChars (
    reinterpret_cast<jstring> (result.l), &isCopy, &jstr);
  char expected3[8] = { 0x7A, 0x00, 0x34, 0xD8, 0x1E, 0xDD, 0x34, 0x6C };
  PLUGIN_TEST (!memcmp (jstr, expected3, 8),
               "string field: 0x7A, 0x00, 0x34, 0xD8, 0x1E, 0xDD, 0x34, 0x6C");

  (*outSecureEnv)->FindClass ("java.awt.Container", &clazz);
  (*outSecureEnv)->FindClass ("java.awt.Component", &superclazz);
  (*outSecureEnv)->GetSuperclass(clazz, &resultclazz);
  PLUGIN_TEST (ID (superclazz) == ID (resultclazz), "CLASS HIERARCHY");
  (*outSecureEnv)->IsAssignableFrom(clazz, superclazz, &resultbool);
  PLUGIN_TEST (resultbool, "HI");
  (*outSecureEnv)->IsAssignableFrom(superclazz, clazz, &resultbool);
  PLUGIN_TEST (!resultbool, "IsAssignableFrom: JNI_FALSE");

  (*outSecureEnv)->FindClass ("java.awt.Container", &clazz);
  (*outSecureEnv)->GetMethodID (clazz, "<init>", "()V", &method);
  (*outSecureEnv)->NewObject (clazz, method, NULL, &newobject);
  (*outSecureEnv)->IsInstanceOf(newobject, clazz, &resultbool);
  PLUGIN_TEST (resultbool, "IsInstanceOf: JNI_TRUE");
  (*outSecureEnv)->FindClass ("java.lang.String", &superclazz);
  (*outSecureEnv)->IsInstanceOf(newobject, superclazz, &resultbool);
  PLUGIN_TEST (!resultbool, "HI");

  // FIXME: test NewString and NewStringUTF
#endif
  // FIXME: call set accessible: these methods should ignore access
  // permissions (public/protected/package private/private -> public)

  // Test multi-dimentional array passing/return.

  // WRITE EXCEPTION HANDLING.
  // WRITE ARRAY ELEMENT SETTING/GETTING.

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginFactory::SpendTime (PRUint32 timeMillis)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::UnwrapJavaWrapper (JNIEnv* jenv, jobject jobj,
                                         PLUGIN_JAVASCRIPT_TYPE* obj)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
IcedTeaPluginFactory::DisplayFailureDialog ()
{
  PLUGIN_TRACE_FACTORY ();
  GtkWidget* dialog = NULL;

  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   FAILURE_MESSAGE,
                                   appletviewer_executable);
  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

}

NS_IMPL_ISUPPORTS2 (IcedTeaPluginInstance, nsIPluginInstance,
                    nsIJVMPluginInstance)

NS_IMETHODIMP
IcedTeaPluginInstance::Initialize (nsIPluginInstancePeer* aPeer)
{
  PLUGIN_TRACE_INSTANCE ();

  // Send applet tag message to appletviewer.
  // FIXME: nsCOMPtr
  char const* documentbase;
  unsigned int i = 0;
  nsresult result = NS_OK;

  nsCOMPtr<nsIPluginTagInfo2> taginfo = do_QueryInterface (aPeer);
  if (!taginfo)
    {
      PLUGIN_ERROR ("Documentbase retrieval failed."
                    "  Browser not Mozilla-based?");
      result = NS_ERROR_FAILURE;
    }
  taginfo->GetDocumentBase (&documentbase);
  if (!documentbase)
    {
      PLUGIN_ERROR ("Documentbase retrieval failed."
                    "  Browser not Mozilla-based?");
      return NS_ERROR_FAILURE;
    }

  char const* appletTag = NULL;
  taginfo->GetTagText (&appletTag);

  nsCString tagMessage (instanceIdentifierPrefix);
  tagMessage += "tag ";
  tagMessage += documentbase;
  tagMessage += " ";
  tagMessage += appletTag;
  tagMessage += "</embed>";
  factory->SendMessageToAppletViewer (tagMessage);

  // Set back-pointer to peer instance.
  printf ("SETTING PEER!!!: %p\n", aPeer);
  peer = aPeer;
  NS_ADDREF (aPeer);
  printf ("DONE SETTING PEER!!!: %p\n", aPeer);

//  if (factory->codebase_map[nsCString(documentbase)] != NULL)
//  {
//	  printf("Found %s in map and it is %s\n", nsCString(documentbase), factory->codebase_map[nsCString(documentbase)].get());
//
//  }

  nsCString dbase(documentbase);
  if (factory->codebase_map.find(dbase) != factory->codebase_map.end())
  {
	factory->codebase_map[dbase] += ",";
    factory->codebase_map[dbase].AppendInt(instance_identifier);

    printf("Appended: %s to %s\n", factory->codebase_map[dbase].get(), documentbase);

  } else 
  {
	nsCString str;
	str.AppendInt(instance_identifier);
    factory->codebase_map[dbase] = str;

	printf("Creating and adding %s to %s and we now have: %s\n", str.get(), documentbase, factory->codebase_map.find(dbase)->second.get());
  }

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginInstance::GetPeer (nsIPluginInstancePeer** aPeer)
{

  PRBool processed;
  nsresult result;
  while (!peer)
    {
      result = factory->current->ProcessNextEvent(PR_TRUE, &processed);
      PLUGIN_CHECK_RETURN ("wait for peer: process next event", result);
    }

  printf ("GETTING PEER!!!: %p\n", peer);
  *aPeer = peer;
  // FIXME: where is this unref'd?
  NS_ADDREF (peer);
  printf ("DONE GETTING PEER!!!: %p, %p\n", peer, *aPeer);
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginInstance::Start ()
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginInstance::Stop ()
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginInstance::Destroy ()
{
  PLUGIN_TRACE_INSTANCE ();

  nsCString destroyMessage (instanceIdentifierPrefix);
  destroyMessage += "destroy";
  factory->SendMessageToAppletViewer (destroyMessage);

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginInstance::SetWindow (nsPluginWindow* aWindow)
{
  PLUGIN_TRACE_INSTANCE ();

  // Simply return if we receive a NULL window.
  if ((aWindow == NULL) || (aWindow->window == NULL))
    {
      PLUGIN_DEBUG ("Instance::SetWindow: got NULL window.");

      return NS_OK;
    }

  if (window_handle)
    {

       if (initialized == PR_FALSE) 
	     {

            printf("IcedTeaPluginInstance::SetWindow: Instance %p waiting for initialization...\n", this);

           while (initialized == PR_FALSE) {
              PROCESS_PENDING_EVENTS;
            }

            printf("Instance %p initialization complete...\n", this);
       }

      // The window already exists.
      if (window_handle == aWindow->window)
	{
          // The parent window is the same as in previous calls.
          PLUGIN_DEBUG ("Instance::SetWindow: window already exists.");

          // The window is the same as it was for the last
          // SetWindow call.
          if (aWindow->width != window_width)
            {
              PLUGIN_DEBUG ("Instance::SetWindow: window width changed.");
              // The width of the plugin window has changed.

              // Send the new width to the appletviewer.
              nsCString widthMessage (instanceIdentifierPrefix);
              widthMessage += "width ";
              widthMessage.AppendInt (aWindow->width);
              factory->SendMessageToAppletViewer (widthMessage);

              // Store the new width.
              window_width = aWindow->width;
            }

          if (aWindow->height != window_height)
            {
              PLUGIN_DEBUG ("Instance::SetWindow: window height changed.");
              // The height of the plugin window has changed.

              // Send the new height to the appletviewer.
              nsCString heightMessage (instanceIdentifierPrefix);
              heightMessage += "height ";
              heightMessage.AppendInt (aWindow->height);
              factory->SendMessageToAppletViewer (heightMessage);

              // Store the new height.
              window_height = aWindow->height;
            }
	}
      else
	{
	  // The parent window has changed.  This branch does run but
	  // doing nothing in response seems to be sufficient.
	  PLUGIN_DEBUG ("Instance::SetWindow: parent window changed.");
	}
    }
  else
    {
      PLUGIN_DEBUG ("Instance::SetWindow: setting window.");

      nsCString windowMessage (instanceIdentifierPrefix);
      windowMessage += "handle ";
      windowMessage.AppendInt (reinterpret_cast<PRInt64>
                               (aWindow->window));
      factory->SendMessageToAppletViewer (windowMessage);

      // Store the window handle.
      window_handle = aWindow->window;
    }

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginInstance::NewStream (nsIPluginStreamListener** aListener)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginInstance::Print (nsPluginPrint* aPlatformPrint)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginInstance::GetValue (nsPluginInstanceVariable aVariable,
                                 void* aValue)
{
  PLUGIN_TRACE_INSTANCE ();
  nsresult result = NS_OK;

  switch (aVariable)
    {
    case nsPluginInstanceVariable_WindowlessBool:
      *static_cast<PRBool*> (aValue) = PR_FALSE;
      break;
    case nsPluginInstanceVariable_TransparentBool:
      *static_cast<PRBool*> (aValue) = PR_FALSE;
      break;
    case nsPluginInstanceVariable_DoCacheBool:
      *static_cast<PRBool*> (aValue) = PR_FALSE;
      break;
    case nsPluginInstanceVariable_CallSetWindowAfterDestroyBool:
      *static_cast<PRBool*> (aValue) = PR_FALSE;
      break;
    case nsPluginInstanceVariable_NeedsXEmbed:
      *static_cast<PRBool*> (aValue) = PR_TRUE;
      break;
    case nsPluginInstanceVariable_ScriptableInstance:
      // Fall through.
    case nsPluginInstanceVariable_ScriptableIID:
      // Fall through.
    default:
      result = NS_ERROR_INVALID_ARG;
      PLUGIN_ERROR ("Unknown plugin value");
    }

  return result;
}

NS_IMETHODIMP
IcedTeaPluginInstance::HandleEvent (nsPluginEvent* aEvent, PRBool* aHandled)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginInstance::GetJavaObject (jobject* object)
{
  PLUGIN_TRACE_INSTANCE ();

  // wait for instance to initialize

  if (initialized == PR_FALSE) 
    {

      printf("IcedTeaPluginInstance::SetWindow: Instance %p waiting for initialization...\n", this);

      while (initialized == PR_FALSE) {
        PROCESS_PENDING_EVENTS;
//      printf("waiting for java object\n");
      }

      printf("Instance %p initialization complete...\n", this);
    }
 
  return factory->GetJavaObject (instance_identifier, object);
}


NS_IMETHODIMP
IcedTeaPluginFactory::GetJavaObject (PRUint32 instance_identifier,
                                     jobject* object)
{
  // add stub to hash table, mapping index to object_stub pointer.  by
  // definition jobject is a pointer, so our jobject representation
  // has to be real pointers.

  // ask Java for index of CODE class
  object_identifier_return = 0;

  int reference = 0;

  nsCString objectMessage ("instance ");
  objectMessage.AppendInt (instance_identifier);
  objectMessage += " reference ";
  objectMessage.AppendInt (reference);
  objectMessage += " GetJavaObject";
  printf ("Sending object message: %s\n", objectMessage.get());
  result_map[reference] = new ResultContainer();
  SendMessageToAppletViewer (objectMessage);

  PRBool processed = PR_FALSE;
  nsresult result = NS_OK;

  // wait for result
  while (object_identifier_return == 0) {
	  current->ProcessNextEvent(PR_TRUE, &processed);
  }

  printf ("GOT JAVA OBJECT IDENTIFIER: %d\n", object_identifier_return);
  if (object_identifier_return == 0)
    printf ("WARNING: received object identifier 0\n");

  *object = references.ReferenceObject (object_identifier_return);

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaPluginInstance::GetText (char const** result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaPluginFactory::OnInputStreamReady (nsIAsyncInputStream* aStream)
{
  PLUGIN_TRACE_INSTANCE ();

  // FIXME: change to NSCString.  Why am I getting symbol lookup errors?
  // /home/fitzsim/sources/mozilla/dist/bin/firefox-bin: symbol lookup error:
  // /usr/lib/jvm/java-1.7.0-icedtea-1.7.0.0/jre/lib/i386/IcedTeaPlugin.so:
  // undefined symbol: _ZNK10nsACString12BeginReadingEv
  char message[10000];
  message[0] = 0;
  char byte = 0;
  PRUint32 readCount = 0;
  int index = 0;

  printf ("ONINPUTSTREAMREADY 1 %p\n", current_thread ());
  // Omit return value checking for speed.
  input->Read (&byte, 1, &readCount);
  if (readCount != 1)
    {
      PLUGIN_ERROR ("failed to read next byte");
      return NS_ERROR_FAILURE;
    }
  while (byte != 0)
    {
      message[index++] = byte;
      // Omit return value checking for speed.
      nsresult result = input->Read (&byte, 1, &readCount);
      if (readCount != 1)
        {
          PLUGIN_ERROR ("failed to read next byte");
          return NS_ERROR_FAILURE;
        }
    }
  message[index] = byte;

  printf ("  PIPE: plugin read: %s\n", message);


  // push message to queue
  printf("Got response. Processing... %s\n", message);
  PR_EnterMonitor(jvmMsgQueuePRMonitor);
  printf("Acquired lock on queue\n");
  jvmMsgQueue.push(nsCString(message));
  printf("Pushed to queue\n");
  PR_ExitMonitor(jvmMsgQueuePRMonitor);

  // poke process thread
  PRThread *prThread;
  processThread->GetPRThread(&prThread);
  printf("Interrupting process thread...\n");
  PRStatus res = PR_Interrupt(prThread);
  printf("Handler event dispatched\n");

  nsresult result = async->AsyncWait (this, 0, 0, current);
  PLUGIN_CHECK_RETURN ("re-add async wait", result);

  return NS_OK;
}

#include <nsIWebNavigation.h>
#include <nsServiceManagerUtils.h>
#include <nsIExternalProtocolService.h>
#include <nsNetUtil.h>

void
IcedTeaPluginFactory::HandleMessage (nsCString const& message)
{
  PLUGIN_DEBUG_TWO ("received message:", message.get());

  nsresult conversionResult;
  PRUint32 space;
  char msg[message.Length()];
  char *pch;

  strcpy(msg, message.get());
  pch = strtok (msg, " ");
  nsDependentCSubstring prefix(pch, strlen(pch));
  pch = strtok (NULL, " ");
  PRUint32 identifier = nsDependentCSubstring(pch, strlen(pch)).ToInteger (&conversionResult);
  PRUint32 reference = -1;

  if (strstr(message.get(), "reference") != NULL) {
	  pch = strtok (NULL, " "); // skip "reference" literal
	  pch = strtok (NULL, " ");
	  reference = nsDependentCSubstring(pch, strlen(pch)).ToInteger (&conversionResult);
  }

  pch = strtok (NULL, " ");
  nsDependentCSubstring command(pch, strlen(pch));
  pch = strtok (NULL, " ");

  nsDependentCSubstring rest("", 0);
  while (pch != NULL) {
	rest += pch;
	pch = strtok (NULL, " ");

	if (pch != NULL)
		rest += " ";
  }

  printf("Parse results: prefix: %s, identifier: %d, reference: %d, command: %s, rest: %s\n", (nsCString (prefix)).get(), identifier, reference, (nsCString (command)).get(), (nsCString (rest)).get());

  if (prefix == "instance")
    {
      if (command == "status")
        {
          IcedTeaPluginInstance* instance = NULL;
          instances.Get (identifier, &instance);
          if (instance != 0)
		  {
            instance->peer->ShowStatus (nsCString (rest).get ());

          }
        }
      else if (command == "initialized")
        {
          IcedTeaPluginInstance* instance = NULL;
          instances.Get (identifier, &instance);
          if (instance != 0) {
			printf("Setting instance.initialized for %p from %d ", instance, instance->initialized);
            instance->initialized = PR_TRUE;
			printf("to %d...\n", instance->initialized);
		  }
		}
      else if (command == "url")
        {
          IcedTeaPluginInstance* instance = NULL;
          instances.Get (identifier, &instance);
          if (instance != 0)
            {
              space = rest.FindChar (' ');
              nsDependentCSubstring url = Substring (rest, 0, space);
              nsDependentCSubstring target = Substring (rest, space + 1);
              nsCOMPtr<nsPIPluginInstancePeer> ownerGetter =
                do_QueryInterface (instance->peer);
              nsIPluginInstanceOwner* owner = nsnull;
              ownerGetter->GetOwner (&owner);
			  printf("Calling GetURL with %s and %s\n", nsCString (url).get (), nsCString (target).get ());
              nsCOMPtr<nsIRunnable> event = new GetURLRunnable (instance->peer,
													 nsCString (url).get (),
													 nsCString (target).get ());
              current->Dispatch(event, nsIEventTarget::DISPATCH_NORMAL);
            }
        }
      else if (command == "GetWindow")
        {
          IcedTeaPluginInstance* instance = NULL;
          instances.Get (identifier, &instance);

		  printf("GetWindow instance: %d\n", instance);
          if (instance != 0)
            {
              nsCOMPtr<nsIRunnable> event =
                new IcedTeaRunnableMethod<IcedTeaPluginInstance>
                (instance,
                 &IcedTeaPluginInstance::IcedTeaPluginInstance::GetWindow);
              NS_DispatchToMainThread (event);
            }
        }
      else if (command == "GetMember")
        {
          printf ("POSTING GetMember\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring nameID = Substring (rest, space + 1);
          name_identifier = nameID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse name id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::GetMember);
          NS_DispatchToMainThread (event);
          printf ("POSTING GetMember DONE\n");
        }
      else if (command == "SetMember")
        {
          printf ("POSTING SetMember\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring nameAndValue = Substring (rest, space + 1);
          space = nameAndValue.FindChar (' ');
          nsDependentCSubstring nameID = Substring (nameAndValue, 0, space);
          // FIXME: these member variables need to be keyed on thread id
          name_identifier = nameID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse name id", conversionResult);
          nsDependentCSubstring valueID = Substring (nameAndValue, space + 1);
          value_identifier = valueID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse value id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::SetMember);
          NS_DispatchToMainThread (event);
          printf ("POSTING SetMember DONE\n");
        }
      else if (command == "GetSlot")
        {
          printf ("POSTING GetSlot\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring indexStr = Substring (rest, space + 1);
          slot_index = indexStr.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse name id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::GetSlot);
          NS_DispatchToMainThread (event);
          printf ("POSTING GetSlot DONE\n");
        }
      else if (command == "SetSlot")
        {
          printf ("POSTING SetSlot\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring nameAndValue = Substring (rest, space + 1);
          space = nameAndValue.FindChar (' ');
          nsDependentCSubstring indexStr = Substring (nameAndValue, 0, space);
          slot_index = indexStr.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse name id", conversionResult);
          nsDependentCSubstring valueID = Substring (nameAndValue, space + 1);
          value_identifier = valueID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse value id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::SetSlot);
          NS_DispatchToMainThread (event);
          printf ("POSTING SetSlot DONE\n");
        }
      else if (command == "Eval")
        {
          printf ("POSTING Eval\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring stringID = Substring (rest, space + 1);
          string_identifier = stringID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse string id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::Eval);
          NS_DispatchToMainThread (event);
          printf ("POSTING Eval DONE\n");
        }
      else if (command == "RemoveMember")
        {
          printf ("POSTING RemoveMember\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring nameID = Substring (rest, space + 1);
          name_identifier = nameID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse name id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::RemoveMember);
          NS_DispatchToMainThread (event);
          printf ("POSTING RemoveMember DONE\n");
        }
      else if (command == "Call")
        {
          printf ("POSTING Call\n");
          space = rest.FindChar (' ');
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = javascriptID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);
          nsDependentCSubstring nameAndArgs = Substring (rest, space + 1);
          space = nameAndArgs.FindChar (' ');
          nsDependentCSubstring nameID = Substring (nameAndArgs, 0, space);
          name_identifier = nameID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse method name id", conversionResult);
          nsDependentCSubstring argsID = Substring (nameAndArgs, space + 1);
          args_identifier = argsID.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse args id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::Call);
          NS_DispatchToMainThread (event);
          printf ("POSTING Call DONE\n");
        }
      else if (command == "Finalize")
        {
          printf ("POSTING Finalize\n");
          nsDependentCSubstring javascriptID = Substring (rest, 0, space);
          javascript_identifier = rest.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::Finalize);
          NS_DispatchToMainThread (event);
          printf ("POSTING Finalize DONE\n");
        }
      else if (command == "ToString")
        {
          printf ("POSTING ToString\n");
          javascript_identifier = rest.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse javascript id", conversionResult);

          nsCOMPtr<nsIRunnable> event =
            new IcedTeaRunnableMethod<IcedTeaPluginFactory>
            (this,
             &IcedTeaPluginFactory::IcedTeaPluginFactory::ToString);
          NS_DispatchToMainThread (event);
          printf ("POSTING ToString DONE\n");
        }
      else if (command == "Error")
        {
			printf("Error occured. Setting error flag for container @ %d to true\n", reference);
			result_map[reference]->errorOccured = PR_TRUE;
		}
    }
  else if (prefix == "context")
    {
      // FIXME: switch context to identifier.

      //printf ("HandleMessage: XXX%sXXX\n", nsCString (command).get ());
      if (command == "GetJavaObject")
        {
          // FIXME: undefine XPCOM_GLUE_AVOID_NSPR?
          // object_identifier_return = rest.ToInteger (&result);
          // FIXME: replace with returnIdentifier ?
          object_identifier_return = rest.ToInteger (&conversionResult);
          printf("Patrsed integer: %d\n", object_identifier_return);
          PLUGIN_CHECK ("parse integer", conversionResult);

        }
      else if (command == "FindClass"
               || command == "GetSuperclass"
               || command == "IsAssignableFrom"
               || command == "IsInstanceOf"
               || command == "GetStaticMethodID"
               || command == "GetMethodID"
               || command == "GetStaticFieldID"
               || command == "GetFieldID"
               || command == "GetObjectClass"
               || command == "NewObject"
               || command == "NewString"
               || command == "NewStringUTF"
               || command == "GetObjectArrayElement"
               || command == "NewObjectArray"
               || command == "ExceptionOccurred"
               || command == "NewGlobalRef"
               || command == "NewArray")
        {
		  result_map[reference]->returnIdentifier = rest.ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse integer", conversionResult);
          printf ("GOT RETURN IDENTIFIER %d\n", result_map[reference]->returnIdentifier);

        }
      else if (command == "GetField"
               || command == "GetStaticField"
               || command == "CallStaticMethod"
               || command == "GetArrayLength"
               || command == "GetStringUTFLength"
               || command == "GetStringLength"
               || command == "CallMethod")
        {
//          if (returnValue != "")
//            PLUGIN_ERROR ("Return value already defined.");
          
		   result_map[reference]->returnValue = rest; 
           printf ("PLUGIN GOT RETURN VALUE: %s\n", result_map[reference]->returnValue.get());
        }
      else if (command == "GetStringUTFChars")
        {
//          if (returnValue != "")
//            PLUGIN_ERROR ("Return value already defined.");


          nsCString returnValue("");

          // Read byte stream into return value.
          PRUint32 offset = 0;
          PRUint32 previousOffset = 0;

          offset = rest.FindChar (' ');
          int length = Substring (rest, 0,
                                  offset).ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse integer", conversionResult);

          for (int i = 0; i < length; i++)
            {
              previousOffset = offset + 1;
              offset = rest.FindChar (' ', previousOffset);
              returnValue += static_cast<char>
                (Substring (rest, previousOffset,
                            offset - previousOffset).ToInteger (&conversionResult, 16));
              PLUGIN_CHECK ("parse integer", conversionResult);
            }
		  result_map[reference]->returnValue = returnValue;
          printf ("PLUGIN GOT RETURN UTF-8 STRING: %s\n", result_map[reference]->returnValue.get ());
        }
      else if (command == "GetStringChars")
        {
 //         if (!returnValueUCS.IsEmpty ())
//            PLUGIN_ERROR ("Return value already defined.");

          // Read byte stream into return value.
		  nsString returnValueUCS;
		  returnValueUCS.Truncate();

          PRUint32 offset = 0;
          PRUint32 previousOffset = 0;

          offset = rest.FindChar (' ');

          int length = Substring (rest, 0,
                                  offset).ToInteger (&conversionResult);
          PLUGIN_CHECK ("parse integer", conversionResult);
          for (int i = 0; i < length; i++)
            {
              previousOffset = offset + 1;
              offset = rest.FindChar (' ', previousOffset);
              char low = static_cast<char> (
                                            Substring (rest, previousOffset,
                                                       offset - previousOffset).ToInteger (&conversionResult, 16));
              PLUGIN_CHECK ("parse integer", conversionResult);
              previousOffset = offset + 1;
              offset = rest.FindChar (' ', previousOffset);
              char high = static_cast<char> (
                                             Substring (rest, previousOffset,
                                                        offset - previousOffset).ToInteger (&conversionResult, 16));
              PLUGIN_CHECK ("parse integer", conversionResult);
              // FIXME: swap on big-endian systems.
              returnValueUCS += static_cast<PRUnichar> ((high << 8) | low);
	      std::cout << "High: " << high << " Low: " << low << " RVUCS: " << returnValueUCS.get() << std::endl;
            }
          printf ("PLUGIN GOT RETURN UTF-16 STRING: %d: ",
                  returnValueUCS.Length());
          for (int i = 0; i < returnValueUCS.Length(); i++)
            {
              if ((returnValueUCS[i] >= 'A'
                   && returnValueUCS[i] <= 'Z')
                  || (returnValueUCS[i] >= 'a'
                      && returnValueUCS[i] <= 'z')
                  || (returnValueUCS[i] >= '0'
                      && returnValueUCS[i] <= '9'))
                printf ("%c", returnValueUCS[i]);
              else
                printf ("?");
            }
          printf ("\n");
		  result_map[reference]->returnValueUCS = returnValueUCS;

        }
      // Do nothing for: SetStaticField, SetField, ExceptionClear,
      // DeleteGlobalRef, DeleteLocalRef
    }
}

void IcedTeaPluginFactory::ProcessMessage ()
{
	while (true) {
		PR_Sleep(1000);

		// If there was an interrupt, clear it
		PR_ClearInterrupt();

		// Was I interrupted for shutting down?
		if (shutting_down == PR_TRUE) {
			break;
		}

		// Nope. Ok, is there work to do?
		if (!jvmMsgQueue.empty())
		    ConsumeMsgFromJVM();

		// All done. Now let's process pending events

		// Were there new events dispatched?
		
		PRBool this_has_pending, curr_has_pending, processed = PR_FALSE;
		PRBool continue_processing = PR_TRUE;

		while (continue_processing == PR_TRUE) {

		  processThread->HasPendingEvents(&this_has_pending);
		  if (this_has_pending == PR_TRUE) {
			  processThread->ProcessNextEvent(PR_TRUE, &processed);
			  printf("Pending event processed (this) ... %d\n", processed);
		  }

		  current->HasPendingEvents(&curr_has_pending);
		  if (curr_has_pending == PR_TRUE) {
			  current->ProcessNextEvent(PR_TRUE, &processed);
			  printf("Pending event processed (current) ... %d\n", processed);
		  }

		  if (this_has_pending != PR_TRUE && curr_has_pending != PR_TRUE) {
			  continue_processing = PR_FALSE;
		  }
		}
	}

}

void IcedTeaPluginFactory::ConsumeMsgFromJVM ()
{
	PLUGIN_TRACE_INSTANCE ();

	while (!jvmMsgQueue.empty()) {

    	PR_EnterMonitor(jvmMsgQueuePRMonitor);
		nsCString message = jvmMsgQueue.front();
		jvmMsgQueue.pop();
    	PR_ExitMonitor(jvmMsgQueuePRMonitor);

		printf("Processing %s from JVM\n", message.get());
		HandleMessage (message);
		printf("Processing complete\n");
	}
}

/**
 *
 * JNI I/O code
 *

#include <jni.h>

typedef jint (JNICALL *CreateJavaVM_t)(JavaVM **pvm, void **env, void *args);

void IcedTeaPluginFactory::InitJVM ()
{

  JavaVMOption options[2];
  JavaVMInitArgs vm_args;
  long result;
  jmethodID mid;
  jfieldID fid;
  jobject jobj;
  int i, asize;

  void *handle = dlopen(libjvm_so, RTLD_NOW);
  if (!handle) {
    printf("Cannot open library: %s\n", dlerror());
  }

  options[0].optionString = ".";
  options[1].optionString = "-Djava.compiler=NONE";
//  options[2].optionString = "-Xdebug";
//  options[3].optionString = "-Xagent";
//  options[4].optionString = "-Xrunjdwp:transport=dt_socket,address=8787,server=y,suspend=n";

  vm_args.version = JNI_VERSION_1_2;
  vm_args.options = options;
  vm_args.nOptions = 2;
  vm_args.ignoreUnrecognized = JNI_TRUE;

  PLUGIN_DEBUG("invoking vm...\n");

  PR_EnterMonitor(jvmPRMonitor);

  CreateJavaVM_t JNI_CreateJavaVM = (CreateJavaVM_t) dlsym(handle, "JNI_CreateJavaVM");
  result = (*JNI_CreateJavaVM)(&jvm,(void **)&javaEnv, &vm_args);
  if(result == JNI_ERR ) {
    printf("Error invoking the JVM");
	exit(1);
    //return NS_ERROR_FAILURE;
  }

  PLUGIN_DEBUG("Looking for the PluginMain constructor...");

  javaPluginClass = (javaEnv)->FindClass("Lsun/applet/PluginMain;");
  if( javaPluginClass == NULL ) {
    printf("can't find class PluginMain\n");
	exit(1);
    //return NS_ERROR_FAILURE;
  }
  (javaEnv)->ExceptionClear();
  mid=(javaEnv)->GetMethodID(javaPluginClass, "<init>", "()V");

  if( mid == NULL ) {
    printf("can't find method init\n");
	exit(1);
    //return NS_ERROR_FAILURE;
  }

  PLUGIN_DEBUG("Creating PluginMain object...");

  javaPluginObj=(javaEnv)->NewObject(javaPluginClass, mid);

  if( javaPluginObj == NULL ) {
    printf("can't create jobj\n");
	exit(1);
    //return NS_ERROR_FAILURE;
  }

  PLUGIN_DEBUG("PluginMain object created...");

  postMessageMID = (javaEnv)->GetStaticMethodID(javaPluginClass, "postMessage", "(Ljava/lang/String;)V");

  if( postMessageMID == NULL ) {
    printf("can't find method postMessage(Ljava/lang/String;)V\n");
	exit(1);
  }

  getMessageMID = (javaEnv)->GetStaticMethodID(javaPluginClass, "getMessage", "()Ljava/lang/String;");

  if( getMessageMID == NULL ) {
    printf("can't find method getMessage()Ljava/lang/String;\n");
	exit(1);
  }

  jvm->DetachCurrentThread();

  printf("VM Invocation complete, detached");

  PR_ExitMonitor(jvmPRMonitor);

  // Start another thread to periodically poll for available messages

  nsCOMPtr<nsIRunnable> readThreadEvent =
							new IcedTeaRunnableMethod<IcedTeaPluginFactory>
							(this, &IcedTeaPluginFactory::IcedTeaPluginFactory::ReadFromJVM);

  NS_NewThread(getter_AddRefs(readThread), readThreadEvent);

  nsCOMPtr<nsIRunnable> processMessageEvent =
							new IcedTeaRunnableMethod<IcedTeaPluginFactory>
							(this, &IcedTeaPluginFactory::IcedTeaPluginFactory::ProcessMessage);

  NS_NewThread(getter_AddRefs(processThread), processMessageEvent);


  //printf("PluginMain initialized...\n");
  //(jvm)->DestroyJavaVM();
  //dlclose(handle);
}

void IcedTeaPluginFactory::ReadFromJVM ()
{

	PLUGIN_TRACE_INSTANCE ();

	int noResponseCycles = 20;

	const char *message;
	int responseSize;
	jstring response;

	while (true) {

		// Lock, attach, read, detach, unlock
		PR_EnterMonitor(jvmPRMonitor);
		(jvm)->AttachCurrentThread((void**)&javaEnv, NULL);

		response = (jstring) (javaEnv)->CallStaticObjectMethod(javaPluginClass, getMessageMID);
		responseSize = (javaEnv)->GetStringLength(response);

		message = responseSize > 0 ? (javaEnv)->GetStringUTFChars(response, NULL) : "";
		(jvm)->DetachCurrentThread();
		PR_ExitMonitor(jvmPRMonitor);

		if (responseSize > 0) {

			noResponseCycles = 0;

			PR_EnterMonitor(jvmMsgQueuePRMonitor);

			printf("Async processing: %s\n", message);
			jvmMsgQueue.push(nsCString(message));

			PR_ExitMonitor(jvmMsgQueuePRMonitor);
	
			// poke process thread
			PRThread *prThread;
			processThread->GetPRThread(&prThread);

			printf("Interrupting process thread...\n");
			PRStatus res = PR_Interrupt(prThread);

			// go back to bed
			PR_Sleep(PR_INTERVAL_NO_WAIT);
		} else {
			//printf("Async processor sleeping...\n");
            if (noResponseCycles >= 5) {
			    PR_Sleep(1000);
			} else {
				PR_Sleep(PR_INTERVAL_NO_WAIT);
			}

            noResponseCycles++;
		}
	}
}

void IcedTeaPluginFactory::IcedTeaPluginFactory::WriteToJVM(nsCString& message)
{

  PLUGIN_TRACE_INSTANCE ();

  PR_EnterMonitor(jvmPRMonitor);

  (jvm)->AttachCurrentThread((void**)&javaEnv, NULL);

  PLUGIN_DEBUG("Sending to VM:");
  PLUGIN_DEBUG(message.get());
  (javaEnv)->CallStaticVoidMethod(javaPluginClass, postMessageMID, (javaEnv)->NewStringUTF(message.get()));
  PLUGIN_DEBUG("... sent!");

  (jvm)->DetachCurrentThread();
  PR_ExitMonitor(jvmPRMonitor);

  return;

  // Try sync read first. Why you ask? Let me tell you why! because attaching
  // and detaching to the jvm is very expensive. In a standard run, 
  // ReadFromJVM(), takes up 96.7% of the time, of which 66.5% is spent 
  // attaching, and 30.7% is spent detaching. 

  int responseSize;
  jstring response;
  int tries = 0;
  int maxTries = 100;
  const char* retMessage;

  responseSize = 1;
  PRBool processed = PR_FALSE;

  while (responseSize > 0 || tries < maxTries) {

      fflush(stdout);
      fflush(stderr);

	  //printf("trying... %d\n", tries);
	  response = (jstring) (javaEnv)->CallStaticObjectMethod(javaPluginClass, getMessageMID);
	  responseSize = (javaEnv)->GetStringLength(response);

	  retMessage = (javaEnv)->GetStringUTFChars(response, NULL);

	  if (responseSize > 0) {

		    printf("Got response. Processing... %s\n", retMessage);
   
			PR_EnterMonitor(jvmMsgQueuePRMonitor);

		    printf("Acquired lock on queue\n");

			jvmMsgQueue.push(nsCString(retMessage));

		    printf("Pushed to queue\n");

			PR_ExitMonitor(jvmMsgQueuePRMonitor);

            processed = PR_TRUE;

			// If we have a response, bump tries up so we are not looping un-necessarily
			tries = maxTries - 2;
	  } else {
        PR_Sleep(2);
	  }
	  tries++;
  }

  printf("Polling complete...\n");

  (jvm)->DetachCurrentThread();

  PR_ExitMonitor(jvmPRMonitor);

  // wake up asynch read thread if needed

  if (processed == PR_TRUE) {
      // poke process thread
      PRThread *prThread;
      processThread->GetPRThread(&prThread);

      printf("Interrupting process thread...\n");
      PRStatus res = PR_Interrupt(prThread);

      printf("Handler event dispatched\n");
  } else {
      PRThread *prThread;
      readThread->GetPRThread(&prThread);

      printf("Interrupting thread...\n");
      PRStatus res = PR_Interrupt(prThread);
      printf("Interrupted! %d\n", res);
  }

}

*/

nsresult
IcedTeaPluginFactory::StartAppletviewer ()
{

/**
 * JNI I/O code
 *
  InitJVM();
*/

/*
 * Code to initialize separate appletviewer process that communicates over TCP/IP
 */

  PLUGIN_TRACE_INSTANCE ();
  nsresult result;

  nsCOMPtr<nsIComponentManager> manager;
  result = NS_GetComponentManager (getter_AddRefs (manager));
  PLUGIN_CHECK_RETURN ("get component manager", result);

  nsCOMPtr<nsILocalFile> file;
  result = manager->CreateInstanceByContractID (NS_LOCAL_FILE_CONTRACTID,
                                                nsnull,
                                                NS_GET_IID (nsILocalFile),
                                                getter_AddRefs (file));
  PLUGIN_CHECK_RETURN ("create local file", result);

  result = file->InitWithNativePath (nsCString (appletviewer_executable));
  PLUGIN_CHECK_RETURN ("init with path", result);

  result = manager->CreateInstanceByContractID (NS_PROCESS_CONTRACTID,
                                                nsnull,
                                                NS_GET_IID (nsIProcess),
                                                getter_AddRefs (applet_viewer_process));
  PLUGIN_CHECK_RETURN ("create process", result);

  result = applet_viewer_process->Init (file);
  PLUGIN_CHECK_RETURN ("init process", result);

  // FIXME: hard-coded port number.
  char const* args[5] = { "-Xdebug", "-Xnoagent", "-Xrunjdwp:transport=dt_socket,address=8787,server=y,suspend=n", "sun.applet.PluginMain", "50007" };
//  char const* args[2] = { "sun.applet.PluginMain", "50007" };
  result = applet_viewer_process->Run (PR_FALSE, args, 5, nsnull);
  PLUGIN_CHECK_RETURN ("run process", result);

  // start processing thread
  nsCOMPtr<nsIRunnable> processMessageEvent =
							new IcedTeaRunnableMethod<IcedTeaPluginFactory>
							(this, &IcedTeaPluginFactory::IcedTeaPluginFactory::ProcessMessage);

  NS_NewThread(getter_AddRefs(processThread), processMessageEvent);

  return NS_OK;

}

nsresult
IcedTeaPluginFactory::SendMessageToAppletViewer (nsCString& message)
{
  PLUGIN_TRACE_INSTANCE ();

  nsresult result;
  PRBool processed;

  // while outputstream is not yet ready, process next event
  while (!output)
    {
      result = current->ProcessNextEvent(PR_TRUE, &processed);
      PLUGIN_CHECK_RETURN ("wait for output stream initialization: process next event", result);
    }

  printf("Writing to JVM: %s\n", message.get());

/*
 * JNI I/O code
 *
   WriteToJVM(message);
*/

/*
 *
 * Code to send message to separate appletviewer process over TCP/IP
 *
 */

  PRUint32 writeCount = 0;
  // Write trailing \0 as message termination character.
  // FIXME: check that message is a valid UTF-8 string.
  //  printf ("MESSAGE: %s\n", message.get ());
  message.Insert('-',0);
  result = output->Write (message.get (),
                                   message.Length () + 1,
                                   &writeCount);
  PLUGIN_CHECK_RETURN ("wrote bytes", result);
  if (writeCount != message.Length () + 1)
    {
      PLUGIN_ERROR ("Failed to write all bytes.");
      return NS_ERROR_FAILURE;
    }

  result = output->Flush ();
  PLUGIN_CHECK_RETURN ("flushed output", result);

  printf ("  PIPE: plugin wrote: %s\n", message.get ());

  return NS_OK;

}

PRUint32
IcedTeaPluginFactory::RegisterInstance (IcedTeaPluginInstance* instance)
{
  // FIXME: do locking?
  PRUint32 identifier = next_instance_identifier;
  next_instance_identifier++;
  instances.Put (identifier, instance);
  return identifier;
}

void
IcedTeaPluginFactory::UnregisterInstance (PRUint32 instance_identifier)
{
  // FIXME: do locking?
  instances.Remove (instance_identifier);
}

IcedTeaPluginInstance::IcedTeaPluginInstance (IcedTeaPluginFactory* factory)
: window_handle (0),
  window_width (0),
  window_height (0),
  peer(0),
  liveconnect_window (0),
  initialized(PR_FALSE),
  instanceIdentifierPrefix ("")
{
  PLUGIN_TRACE_INSTANCE ();
  IcedTeaPluginInstance::factory = factory;
  instance_identifier = factory->RegisterInstance (this);

  instanceIdentifierPrefix += "instance ";
  instanceIdentifierPrefix.AppendInt (instance_identifier);
  instanceIdentifierPrefix += " ";
}

void
IcedTeaPluginInstance::GetWindow ()
{

  nsresult result;
  printf ("HERE 22: %d\n", liveconnect_window);
  // principalsArray, numPrincipals and securitySupports
  // are ignored by GetWindow.  See:
  //
  // nsCLiveconnect.cpp: nsCLiveconnect::GetWindow
  // jsj_JSObject.c: jsj_enter_js
  // lcglue.cpp: enter_js_from_java_impl
  // so they can all safely be null.
  if (factory->proxyEnv != NULL)
    {
      printf ("HERE 23: %d, %p\n", liveconnect_window, current_thread ());
      result = factory->liveconnect->GetWindow(factory->proxyEnv,
                                               this,
                                               NULL, 0, NULL,
                                               &liveconnect_window);
      PLUGIN_CHECK ("get window", result);
      printf ("HERE 24: %d\n", liveconnect_window);
    }

  printf ("HERE 20: %d\n", liveconnect_window);

  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptGetWindow";
  message += " ";
  message.AppendInt (liveconnect_window);
  factory->SendMessageToAppletViewer (message);
}

IcedTeaPluginInstance::~IcedTeaPluginInstance ()
{
  PLUGIN_TRACE_INSTANCE ();
  factory->UnregisterInstance (instance_identifier);
}

// FIXME: these LiveConnect member functions need to be in instance
// since nsCLiveConnect objects associate themselves with an instance,
// for JavaScript context switching.
void
IcedTeaPluginFactory::GetMember ()
{
  nsresult result;
  printf ("BEFORE GETTING NAMESTRING\n");
  jsize strSize = 0;
  jchar const* nameString;
  jstring name = static_cast<jstring> (references.ReferenceObject (name_identifier));
  ((IcedTeaJNIEnv*) secureEnv)->GetStringLength (name, &strSize);
  ((IcedTeaJNIEnv*) secureEnv)->GetStringChars (name, NULL, &nameString);
  printf ("AFTER GETTING NAMESTRING\n");

  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      printf ("Calling GETMEMBER: %d, %d\n", javascript_identifier, strSize);
      result = liveconnect->GetMember(proxyEnv,
                                      javascript_identifier,
                                      nameString, strSize,
                                      NULL, 0, NULL,
                                      &liveconnect_member);
      PLUGIN_CHECK ("get member", result);
    }

  printf ("GOT MEMBER: %d\n", ID (liveconnect_member));
  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptGetMember";
  message += " ";
  message.AppendInt (ID (liveconnect_member));
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::GetSlot ()
{
  nsresult result;
  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      result = liveconnect->GetSlot(proxyEnv,
                                      javascript_identifier,
                                      slot_index,
                                      NULL, 0, NULL,
                                      &liveconnect_member);
      PLUGIN_CHECK ("get slot", result);
    }

  printf ("GOT SLOT: %d\n", ID (liveconnect_member));
  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptGetSlot";
  message += " ";
  message.AppendInt (ID (liveconnect_member));
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::SetMember ()
{
  nsresult result;
  printf ("BEFORE GETTING NAMESTRING\n");
  jsize strSize = 0;
  jchar const* nameString;
  jstring name = static_cast<jstring> (references.ReferenceObject (name_identifier));
  ((IcedTeaJNIEnv*) secureEnv)->GetStringLength (name, &strSize);
  ((IcedTeaJNIEnv*) secureEnv)->GetStringChars (name, NULL, &nameString);
  printf ("AFTER GETTING NAMESTRING\n");

  jobject value = references.ReferenceObject (value_identifier);
  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      printf ("Calling SETMEMBER: %d, %d\n", javascript_identifier, strSize);
      result = liveconnect->SetMember(proxyEnv,
                                      javascript_identifier,
                                      nameString, strSize,
                                      value,
                                      NULL, 0, NULL);
      PLUGIN_CHECK ("set member", result);
    }

  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptSetMember";
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::SetSlot ()
{
  nsresult result;
  jobject value = references.ReferenceObject (value_identifier);
  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      result = liveconnect->SetSlot(proxyEnv,
                                    javascript_identifier,
                                    slot_index,
                                    value,
                                    NULL, 0, NULL);
      PLUGIN_CHECK ("set slot", result);
    }

  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptSetSlot";
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::Eval ()
{
  nsresult result;
  printf ("BEFORE GETTING NAMESTRING\n");
  jsize strSize = 0;
  jchar const* nameString;
  // FIXME: unreference after SendMessageToAppletViewer call.
  jstring name = static_cast<jstring> (references.ReferenceObject (string_identifier));
  ((IcedTeaJNIEnv*) secureEnv)->GetStringLength (name, &strSize);
  ((IcedTeaJNIEnv*) secureEnv)->GetStringChars (name, NULL, &nameString);
  printf ("AFTER GETTING NAMESTRING\n");

  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      printf ("Calling Eval: %d, %d\n", javascript_identifier, strSize);
      result = liveconnect->Eval(proxyEnv,
                                 javascript_identifier,
                                 nameString, strSize,
                                 NULL, 0, NULL,
                                 &liveconnect_member);
      PLUGIN_CHECK ("eval", result);
    }

  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptEval";
  message += " ";
  message.AppendInt (ID (liveconnect_member));
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::RemoveMember ()
{
  nsresult result;
  printf ("BEFORE GETTING NAMESTRING\n");
  jsize strSize = 0;
  jchar const* nameString;
  jstring name = static_cast<jstring> (references.ReferenceObject (name_identifier));
  ((IcedTeaJNIEnv*) secureEnv)->GetStringLength (name, &strSize);
  ((IcedTeaJNIEnv*) secureEnv)->GetStringChars (name, NULL, &nameString);
  printf ("AFTER GETTING NAMESTRING\n");

  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      printf ("Calling RemoveMember: %d, %d\n", javascript_identifier, strSize);
      result = liveconnect->RemoveMember(proxyEnv,
                                         javascript_identifier,
                                         nameString, strSize,
                                         NULL, 0, NULL);
      PLUGIN_CHECK ("RemoveMember", result);
    }

  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptRemoveMember";
  message += " ";
  message.AppendInt (ID (liveconnect_member));
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::Call ()
{
  nsresult result;
  printf ("BEFORE GETTING NAMESTRING\n");
  jsize strSize = 0;
  jchar const* nameString;
  jstring name = static_cast<jstring> (
    references.ReferenceObject (name_identifier));
  ((IcedTeaJNIEnv*) secureEnv)->GetStringLength (name, &strSize);
  ((IcedTeaJNIEnv*) secureEnv)->GetStringChars (name, NULL, &nameString);
  printf ("AFTER GETTING NAMESTRING\n");
  jobjectArray args = static_cast<jobjectArray> (
    references.ReferenceObject (args_identifier));

  jobject liveconnect_member;
  if (proxyEnv != NULL)
    {
      printf ("CALL: %d, %d\n", javascript_identifier, strSize);
      result = liveconnect->Call(proxyEnv,
                                 javascript_identifier,
                                 nameString, strSize,
                                 args,
                                 NULL, 0, NULL,
                                 &liveconnect_member);
      PLUGIN_CHECK ("call", result);
    }

  printf ("GOT RETURN FROM CALL : %d\n", ID (liveconnect_member));
  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptCall";
  message += " ";
  message.AppendInt (ID (liveconnect_member));
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::Finalize ()
{
  nsresult result;
  if (proxyEnv != NULL)
    {
      printf ("FINALIZE: %d\n", javascript_identifier);
      result = liveconnect->FinalizeJSObject(proxyEnv,
                                             javascript_identifier);
      PLUGIN_CHECK ("finalize", result);
    }

  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptFinalize";
  SendMessageToAppletViewer (message);
}

void
IcedTeaPluginFactory::ToString ()
{
  nsresult result;

  jstring liveconnect_member;
  if (proxyEnv != NULL)
    {
      printf ("Calling ToString: %d\n", javascript_identifier);
      result = liveconnect->ToString(proxyEnv,
                                     javascript_identifier,
                                     &liveconnect_member);
      PLUGIN_CHECK ("ToString", result);
    }

  printf ("ToString: %d\n", ID (liveconnect_member));
  nsCString message ("context ");
  message.AppendInt (0);
  message += " ";
  message += "JavaScriptToString";
  message += " ";
  message.AppendInt (ID (liveconnect_member));
  SendMessageToAppletViewer (message);
}

nsresult
IcedTeaPluginFactory::SetTransport (nsISocketTransport* transport)
{
  PLUGIN_TRACE_INSTANCE ();
  IcedTeaPluginFactory::transport = transport;
//   sink = new IcedTeaEventSink ();
//   nsresult result = transport->SetEventSink (sink, nsnull);
//   PLUGIN_CHECK ("socket event sink", result);
//   return result;
  // FIXME: remove return if EventSink not needed.
  return NS_OK;
}

void
IcedTeaPluginFactory::Connected ()
{
  PLUGIN_TRACE_INSTANCE ();
  connected = PR_TRUE;
}

void
IcedTeaPluginFactory::Disconnected ()
{
  PLUGIN_TRACE_INSTANCE ();
  connected = PR_FALSE;
}

PRBool
IcedTeaPluginFactory::IsConnected ()
{
//  PLUGIN_TRACE_INSTANCE ();
  return connected;
}

NS_IMPL_ISUPPORTS1 (IcedTeaSocketListener, nsIServerSocketListener)

IcedTeaSocketListener::IcedTeaSocketListener (IcedTeaPluginFactory* factory)
{
  PLUGIN_TRACE_LISTENER ();

  IcedTeaSocketListener::factory = factory;
}

IcedTeaSocketListener::~IcedTeaSocketListener ()
{
  PLUGIN_TRACE_LISTENER ();
}

NS_IMETHODIMP
IcedTeaSocketListener::OnSocketAccepted (nsIServerSocket* aServ,
                                         nsISocketTransport* aTransport)
{
  PLUGIN_TRACE_LISTENER ();

  nsresult result = factory->SetTransport (aTransport);
  PLUGIN_CHECK_RETURN ("set transport", result);
  factory->Connected ();

  result = aTransport->OpenOutputStream (nsITransport::OPEN_BLOCKING,
                                        nsnull, nsnull,
                                        getter_AddRefs (factory->output));
  PLUGIN_CHECK_RETURN ("output stream", result);

  result = aTransport->OpenInputStream (0, nsnull, nsnull,
                                       getter_AddRefs (factory->input));
  PLUGIN_CHECK_RETURN ("input stream", result);

  factory->async = do_QueryInterface (factory->input, &result);
  PLUGIN_CHECK_RETURN ("async input stream", result);

  result = factory->async->AsyncWait (factory, 0, 0, factory->current);
  PLUGIN_CHECK_RETURN ("add async wait", result);


  return NS_OK;
}

// FIXME: handle appletviewer crash and shutdown scenarios.
NS_IMETHODIMP
IcedTeaSocketListener::OnStopListening (nsIServerSocket *aServ,
                                        nsresult aStatus)
{
  PLUGIN_TRACE_LISTENER ();

  nsCString shutdownStr("shutdown");
  printf("stop listening: %uld\n", aStatus);

  nsresult result = NS_OK;

  switch (aStatus)
  {
  case NS_ERROR_ABORT:
    factory->SendMessageToAppletViewer(shutdownStr);
    PLUGIN_DEBUG ("appletviewer stopped");
    // FIXME: privatize?
    result = factory->async->AsyncWait (nsnull, 0, 0, factory->current);
    PLUGIN_CHECK_RETURN ("clear async wait", result);
    break;
  default:
    printf ("ERROR %x\n", aStatus);
    PLUGIN_DEBUG ("Listener: Unknown status value.");
  }
  return NS_OK;
}

NS_IMPL_ISUPPORTS1 (IcedTeaEventSink, nsITransportEventSink)

IcedTeaEventSink::IcedTeaEventSink ()
{
  PLUGIN_TRACE_EVENTSINK ();
}

IcedTeaEventSink::~IcedTeaEventSink ()
{
  PLUGIN_TRACE_EVENTSINK ();
}

//static int connected = 0;

NS_IMETHODIMP
IcedTeaEventSink::OnTransportStatus (nsITransport *aTransport,
                                     nsresult aStatus,
                                     PRUint64 aProgress,
                                     PRUint64 aProgressMax)
{
  PLUGIN_TRACE_EVENTSINK ();

  switch (aStatus)
    {
      case nsISocketTransport::STATUS_RESOLVING:
        PLUGIN_DEBUG ("RESOLVING");
        break;
      case nsISocketTransport::STATUS_CONNECTING_TO:
        PLUGIN_DEBUG ("CONNECTING_TO");
        break;
      case nsISocketTransport::STATUS_CONNECTED_TO:
        PLUGIN_DEBUG ("CONNECTED_TO");
        //        connected = 1;
        break;
      case nsISocketTransport::STATUS_SENDING_TO:
        PLUGIN_DEBUG ("SENDING_TO");
        break;
      case nsISocketTransport::STATUS_WAITING_FOR:
        PLUGIN_DEBUG ("WAITING_FOR");
        break;
      case nsISocketTransport::STATUS_RECEIVING_FROM:
        PLUGIN_DEBUG ("RECEIVING_FROM");
        break;
    default:
      PLUGIN_ERROR ("Unknown transport status.");
    }

  return NS_OK;
}

NS_IMPL_ISUPPORTS1 (IcedTeaJNIEnv, nsISecureEnv)

#include <nsCOMPtr.h>
#include <nsIOutputStream.h>
#include <nsISocketTransportService.h>
#include <nsISocketTransport.h>
#include <nsITransport.h>
#include <nsNetCID.h>
#include <nsServiceManagerUtils.h>
#include <iostream>
#include <jvmmgr.h>
#include <nsIPrincipal.h>
#include <nsIScriptSecurityManager.h>
#include <nsIURI.h>
#include <xpcjsid.h>

IcedTeaJNIEnv::IcedTeaJNIEnv (IcedTeaPluginFactory* factory)
: factory (factory)
{
  PLUGIN_TRACE_JNIENV ();
  contextCounter = 1;

  contextCounterPRMonitor = PR_NewMonitor();
}

IcedTeaJNIEnv::~IcedTeaJNIEnv ()
{
  PLUGIN_TRACE_JNIENV ();
}

int
IcedTeaJNIEnv::IncrementContextCounter ()
{

	PLUGIN_TRACE_JNIENV ();

    PR_EnterMonitor(contextCounterPRMonitor);
    contextCounter++;
    PR_ExitMonitor(contextCounterPRMonitor);

	return contextCounter;
}

void
IcedTeaJNIEnv::DecrementContextCounter ()
{
	PLUGIN_TRACE_JNIENV ();

    PR_EnterMonitor(contextCounterPRMonitor);
    contextCounter--;
    PR_ExitMonitor(contextCounterPRMonitor);
}

#include <nsIJSContextStack.h>

nsresult
IcedTeaJNIEnv::GetCurrentContextAddr(char *addr)
{
	return NS_OK;
    PLUGIN_TRACE_JNIENV ();

    // Get JSContext from stack.
    nsCOMPtr<nsIJSContextStack> mJSContextStack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
    if (mJSContextStack) {
        JSContext *cx;
        if (NS_FAILED(mJSContextStack->Peek(&cx)))
            return NS_ERROR_FAILURE;

        printf("Context1: %p\n", cx);

        // address cannot be more than 8 bytes (8 bytes = 64 bits)
		sprintf(addr, "%p", cx);

        printf("Context2: %s\n", addr);
	}

	return NS_OK;
}

nsresult
IcedTeaJNIEnv::GetCurrentPageAddress(const char **addr)
{
	return NS_OK;
    PLUGIN_TRACE_JNIENV ();

    nsIPrincipal *prin;
	nsCOMPtr<nsIScriptSecurityManager> sec_man(do_GetService("@mozilla.org/scriptsecuritymanager;1"));

    if (sec_man) {
    
		PRBool isEnabled = PR_FALSE;
    	sec_man->IsCapabilityEnabled("UniversalBrowserRead", &isEnabled);

		if (isEnabled == PR_FALSE) {
			printf("UniversalBrowserRead is NOT enabled\n");
		} else {
			printf("UniversalBrowserRead IS enabled\n");
		}

    	sec_man->IsCapabilityEnabled("UniversalBrowserWrite", &isEnabled);

		if (isEnabled == PR_FALSE) {
			printf("UniversalBrowserWrite is NOT enabled\n");
		} else {
			printf("UniversalBrowserWrite IS enabled\n");
		}
	}

    if (sec_man)
	{
    	sec_man->GetSubjectPrincipal(&prin);
	} else {
		return NS_ERROR_FAILURE;
	}

   if (prin)
   {
       nsIURI *uri;
       prin->GetURI(&uri);

	   if (uri)
	   {
           nsCAutoString str;
           uri->GetSpec(str);
           NS_CStringGetData(str, addr);
	   } else {
		   return NS_ERROR_FAILURE;
	   }
   } else {
	   return NS_ERROR_FAILURE;
   }


	nsCOMPtr<nsIJSID> js_id(do_GetService("@mozilla.org/js/xpc/ID;1"));
	printf("JS ID is: %s\n", js_id->GetID()->ToString());

    return NS_OK;

}

NS_IMETHODIMP
IcedTeaJNIEnv::NewObject (jclass clazz,
                          jmethodID methodID,
                          jvalue* args,
                          jobject* result,
                          nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_ID (methodID);
  MESSAGE_ADD_ARGS (methodID, args);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jobject, result);
  DecrementContextCounter ();

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::CallMethod (jni_type type,
                           jobject obj,
                           jmethodID methodID,
                           jvalue* args,
                           jvalue* result,
                           nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (obj);
  MESSAGE_ADD_ID (methodID);
  MESSAGE_ADD_ARGS (methodID, args);
  std::cout << "CALLMETHOD -- OBJ: " << obj << " METHOD: " << methodID << " ARGS: " << args << std::endl;
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_VALUE (reference, type, result);
  DecrementContextCounter ();

  return NS_OK;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::CallNonvirtualMethod (jni_type type,
                                     jobject obj,
                                     jclass clazz,
                                     jmethodID methodID,
                                     jvalue* args,
                                     jvalue* result,
                                     nsISecurityContext* ctx)
{

  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsPrintfCString is not exported.
class NS_COM IcedTeaPrintfCString : public nsCString
  {
    typedef nsCString string_type;

    enum { kLocalBufferSize = 15 };

    public:
      explicit IcedTeaPrintfCString( const char_type* format, ... );

    private:
      char_type  mLocalBuffer[ kLocalBufferSize + 1 ];
  };

IcedTeaPrintfCString::IcedTeaPrintfCString( const char_type* format, ... )
  : string_type(mLocalBuffer, 0, NS_STRING_CONTAINER_INIT_DEPEND)
  {
    va_list ap;

    size_type logical_capacity = kLocalBufferSize;
    size_type physical_capacity = logical_capacity + 1;

    va_start(ap, format);
    int len = PR_vsnprintf(mLocalBuffer, physical_capacity, format, ap);
    SetLength (len);
    NS_CStringSetData(*this, mLocalBuffer, len);
    va_end(ap);
  }

char*
IcedTeaJNIEnv::ValueString (jni_type type, jvalue value)
{
  PLUGIN_TRACE_JNIENV ();
  nsCString retstr ("");

  switch (type)
    {
    case jboolean_type:
      retstr += value.z ? "true" : "false";
      break;
    case jbyte_type:
      retstr.AppendInt (value.b & 0x0ff, 16);
      break;
    case jchar_type:
      retstr += value.c;
      break;
    case jshort_type:
      retstr.AppendInt (value.s);
      break;
    case jint_type:
      retstr.AppendInt (value.i);
      break;
    case jlong_type:
      retstr.AppendInt (value.j);
      break;
    case jfloat_type:
      retstr += IcedTeaPrintfCString ("%f", value.f);
      break;
    case jdouble_type:
      retstr += IcedTeaPrintfCString ("%g", value.d);
      break;
    case jobject_type:
      retstr.AppendInt (ID (value.l));
      break;
    case jvoid_type:
      break;
    default:
      break;
    }

  // Freed by calling function.
  return strdup (retstr.get ());
}

jvalue
IcedTeaJNIEnv::ParseValue (jni_type type, nsCString& str)
{
  PLUGIN_TRACE_JNIENV ();
  jvalue retval;
  PRUint32 id;
  char** bytes;
  int low;
  int high;
  int offset;
  nsresult conversionResult;

  switch (type)
    {
    case jboolean_type:
      retval.z = (jboolean) (str == "true");
      break;
    case jbyte_type:
      retval.b = (jbyte) str.ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse int", conversionResult);
      break;
    case jchar_type:
      offset = str.FindChar ('_', 0);
      low = Substring (str, 0, offset).ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse integer", conversionResult);
      high = Substring (str, offset + 1).ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse integer", conversionResult);
      retval.c = ((high << 8) & 0x0ff00) | (low & 0x0ff);
      break;
    case jshort_type:
      // Assume number is in range.
      retval.s = str.ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse int", conversionResult);
      break;
    case jint_type:
      retval.i = str.ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse int", conversionResult);
      break;
    case jlong_type:
      retval.j =  str.ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse int", conversionResult);
      break;
    case jfloat_type:
      retval.f = strtof (str.get (), NULL);
      break;
    case jdouble_type:
      retval.d = strtold (str.get (), NULL);
      break;
    case jobject_type:
      // Have we referred to this object before?
      id = str.ToInteger (&conversionResult);
      PLUGIN_CHECK ("parse int", conversionResult);
      retval.l = factory->references.ReferenceObject (id);
      break;
    case jvoid_type:
      // Clear.
      retval.i = 0;
      break;
    default:
      printf ("WARNING: didn't handle parse type\n");
      break;
    }

  return retval;
}

// FIXME: make ExpandArgs extend nsACString
char*
IcedTeaJNIEnv::ExpandArgs (JNIID* id, jvalue* args)
{
  PLUGIN_TRACE_JNIENV ();
  nsCString retstr ("");

  int i = 0;
  char stopchar = '\0';
  // FIXME: check for end-of-string throughout.
  if (id->signature[0] == '(')
    {
      i = 1;
      stopchar = ')';
    }
 
  // Method.
  int arg = 0;
  char* fl;
  while (id->signature[i] != stopchar)
    {
      switch (id->signature[i])
        {
        case 'Z':
          retstr += args[arg].z ? "true" : "false";
          break;
        case 'B':
          retstr.AppendInt (args[arg].b);
          break;
        case 'C':
          retstr.AppendInt (static_cast<int> (args[arg].c) & 0x0ff);
          retstr += "_";
          retstr.AppendInt ((static_cast<int> (args[arg].c)
                             >> 8) & 0x0ff);
          break;
        case 'S':
          retstr.AppendInt (args[arg].s);
          break;
        case 'I':
	   	  printf("Appending (I @ %d) %d\n", arg, args[arg].i);
          retstr.AppendInt (args[arg].i);
          break;
        case 'J':
	   	  printf("Appending (J @ %d) %d\n", arg, args[arg].i);
          retstr.AppendInt (args[arg].j);
          break;
        case 'F':
          retstr += IcedTeaPrintfCString ("%f", args[arg].f);
          break;
        case 'D':
          retstr += IcedTeaPrintfCString ("%g", args[arg].d);
          break;
        case 'L':
          std::cout << "Appending for L: arg=" << arg << " args[arg].l=" << args[arg].l << std::endl;
          retstr.AppendInt (ID (args[arg].l));
          i++;
          while (id->signature[i] != ';')
            i++;
          break;
        case '[':
          retstr.AppendInt (ID (args[arg].l));
          i++;
          while (id->signature[i] == '[')
            i++;
          if (id->signature[i] == 'L')
            {
              while (id->signature[i] != ';')
                i++;
            }
          else
            {
              if (!(id->signature[i] == 'Z'
                    || id->signature[i] == 'B'
                    || id->signature[i] == 'C'
                    || id->signature[i] == 'S'
                    || id->signature[i] == 'I'
                    || id->signature[i] == 'J'
                    || id->signature[i] == 'F'
                    || id->signature[i] == 'D'))
                PLUGIN_ERROR_TWO ("Failed to parse signature", id->signature);
            }
          break;
        default:
          PLUGIN_ERROR_TWO ("Failed to parse signature", id->signature);
          printf ("FAILED ID: %d\n", id->identifier);
          break;
        }
	
	  retstr += " ";
      i++;
	  arg++;
    }

  // Freed by calling function.
  return strdup (retstr.get ());
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetField (jni_type type,
                         jobject obj,
                         jfieldID fieldID,
                         jvalue* result,
                         nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (obj);
  MESSAGE_ADD_ID (fieldID);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_VALUE (reference, type, result);
  DecrementContextCounter ();

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::SetField (jni_type type,
                         jobject obj,
                         jfieldID fieldID,
                         jvalue val,
                         nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  MESSAGE_CREATE (-1);
  MESSAGE_ADD_TYPE (type);
  MESSAGE_ADD_REFERENCE (obj);
  MESSAGE_ADD_ID (fieldID);
  MESSAGE_ADD_VALUE (fieldID, val);
  MESSAGE_SEND ();

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::CallStaticMethod (jni_type type,
                                 jclass clazz,
                                 jmethodID methodID,
                                 jvalue* args,
                                 jvalue* result,
                                 nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_ID (methodID);
  MESSAGE_ADD_ARGS (methodID, args);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_VALUE (reference, type, result);
  DecrementContextCounter ();

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStaticField (jni_type type,
                               jclass clazz,
                               jfieldID fieldID,
                               jvalue* result,
                               nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_ID (fieldID);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_VALUE (reference, type, result);
  DecrementContextCounter ();

  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::SetStaticField (jni_type type,
                               jclass clazz,
                               jfieldID fieldID,
                               jvalue val,
                               nsISecurityContext* ctx)
{
  PLUGIN_TRACE_JNIENV ();
  MESSAGE_CREATE (-1);
  MESSAGE_ADD_TYPE (type);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_ID (fieldID);
  MESSAGE_ADD_VALUE (fieldID, val);
  MESSAGE_SEND ();

  return NS_OK;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::GetVersion (jint* version)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::DefineClass (char const* name,
                            jobject loader,
                            jbyte const* buf,
                            jsize len,
                            jclass* clazz)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaJNIEnv::FindClass (char const* name,
                          jclass* clazz)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_STRING (name);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jclass, clazz);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetSuperclass (jclass sub,
                              jclass* super)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (sub);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jclass, super);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::IsAssignableFrom (jclass sub,
                                 jclass super,
                                 jboolean* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (sub);
  MESSAGE_ADD_REFERENCE (super);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_BOOLEAN (reference, result);
  DecrementContextCounter ();
  return NS_OK;
}

// IMPLEMENTME.
NS_IMETHODIMP
IcedTeaJNIEnv::Throw (jthrowable obj,
                      jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::ThrowNew (jclass clazz,
                         char const* msg,
                         jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaJNIEnv::ExceptionOccurred (jthrowable* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  // FIXME: potential leak here: when is result free'd?
  MESSAGE_RECEIVE_REFERENCE (reference, jthrowable, result);
  DecrementContextCounter ();
  printf ("GOT RESUlT: %x\n", *result);
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::ExceptionDescribe (void)
{
  PLUGIN_TRACE_JNIENV ();
  // Do nothing.
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::ExceptionClear (void)
{
  PLUGIN_TRACE_JNIENV ();
  MESSAGE_CREATE (-1);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  return NS_OK;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::FatalError (char const* msg)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaJNIEnv::NewGlobalRef (jobject lobj,
                             jobject* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (lobj);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE(reference, jobject, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::DeleteGlobalRef (jobject gref)
{
  PLUGIN_TRACE_JNIENV ();
  MESSAGE_CREATE (-1);
  MESSAGE_ADD_REFERENCE (gref);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  factory->references.UnreferenceObject (ID (gref));
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::DeleteLocalRef (jobject obj)
{
  PLUGIN_TRACE_JNIENV ();
  MESSAGE_CREATE (-1);
  MESSAGE_ADD_REFERENCE (obj);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
//  factory->references.UnreferenceObject (ID (obj));
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::IsSameObject (jobject obj1,
                             jobject obj2,
                             jboolean* result)
{
  PLUGIN_TRACE_JNIENV ();
  *result = (obj1 == NULL && obj2 == NULL) ||
    ((obj1 != NULL && obj2 != NULL)
     && (ID (obj1) == ID (obj2)));
  return NS_OK;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::AllocObject (jclass clazz,
                            jobject* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetObjectClass (jobject obj,
                               jclass* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (obj);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jclass, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::IsInstanceOf (jobject obj,
                             jclass clazz,
                             jboolean* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (obj);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_BOOLEAN (reference, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetMethodID (jclass clazz,
                            char const* name,
                            char const* sig,
                            jmethodID* id)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_STRING (name);
  std::cout << "Args: " << clazz << " " << name << " " << sig << " " << *id << "@" << id << std::endl;
  printf ("SIGNATURE: %s %s %s\n", __func__, name, sig);
  std::cout << "Storing it at: " << id << " Currently it is: " << *id << std::endl;
  MESSAGE_ADD_STRING (sig);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_ID (reference, jmethodID, id, sig);
  DecrementContextCounter ();
  std::cout << "GETMETHODID -- Name: " << name << " SIG: " << sig << " METHOD: " << id << " METHODVAL: " << *id << std::endl;
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetFieldID (jclass clazz,
                           char const* name,
                           char const* sig,
                           jfieldID* id)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_STRING (name);
  printf ("SIGNATURE: %s %s %s\n", __func__, name, sig);
  MESSAGE_ADD_STRING (sig);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_ID (reference, jfieldID, id, sig);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStaticMethodID (jclass clazz,
                                  char const* name,
                                  char const* sig,
                                  jmethodID* id)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_STRING (name);
  printf ("SIGNATURE: %s %s\n", __func__, sig);
  MESSAGE_ADD_STRING (sig);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_ID (reference, jmethodID, id, sig);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStaticFieldID (jclass clazz,
                                 char const* name,
                                 char const* sig,
                                 jfieldID* id)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_STRING (name);
  printf ("SIGNATURE: %s %s\n", __func__, sig);
  MESSAGE_ADD_STRING (sig);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_ID (reference, jfieldID, id, sig);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::NewString (jchar const* unicode,
                          jsize len,
                          jstring* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_SIZE (len);
  MESSAGE_ADD_STRING_UCS (unicode, len);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jstring, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStringLength (jstring str,
                                jsize* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (str);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_SIZE (reference, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStringChars (jstring str,
                               jboolean* isCopy,
                               jchar const** result)
{
  PLUGIN_TRACE_JNIENV ();
  if (isCopy)
    *isCopy = JNI_TRUE;

  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (str);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_STRING_UCS (reference, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::ReleaseStringChars (jstring str,
                                   jchar const* chars)
{
  PLUGIN_TRACE_JNIENV ();
  PR_Free ((void*) chars);
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::NewStringUTF (char const* utf,
                             jstring* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_STRING_UTF (utf);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jstring, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStringUTFLength (jstring str,
                                   jsize* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (str);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_SIZE (reference, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetStringUTFChars (jstring str,
                                  jboolean* isCopy,
                                  char const** result)
{
  PLUGIN_TRACE_JNIENV ();
  if (isCopy)
    *isCopy = JNI_TRUE;

  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (str);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_STRING (reference, char, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::ReleaseStringUTFChars (jstring str,
                                      char const* chars)
{
  PLUGIN_TRACE_JNIENV ();
  PR_Free ((void*) chars);
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetArrayLength (jarray array,
                               jsize* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (array);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_SIZE (reference, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::NewObjectArray (jsize len,
                               jclass clazz,
                               jobject init,
                               jobjectArray* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_SIZE (len);
  MESSAGE_ADD_REFERENCE (clazz);
  MESSAGE_ADD_REFERENCE (init);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jobjectArray, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::GetObjectArrayElement (jobjectArray array,
                                      jsize index,
                                      jobject* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_REFERENCE (array);
  MESSAGE_ADD_SIZE (index);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jobject, result);
  DecrementContextCounter ();
  return NS_OK;
}

NS_IMETHODIMP
IcedTeaJNIEnv::SetObjectArrayElement (jobjectArray array,
                                      jsize index,
                                      jobject val)
{
  PLUGIN_TRACE_JNIENV ();
  MESSAGE_CREATE (-1);
  MESSAGE_ADD_REFERENCE (array);
  MESSAGE_ADD_SIZE (index);
  MESSAGE_ADD_REFERENCE (val);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  return NS_OK;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::NewArray (jni_type element_type,
                         jsize len,
                         jarray* result)
{
  PLUGIN_TRACE_JNIENV ();
  int reference = IncrementContextCounter ();
  MESSAGE_CREATE (reference);
  MESSAGE_ADD_TYPE (element_type);
  MESSAGE_ADD_SIZE (len);
  MESSAGE_SEND ();
  printf("MSG SEND COMPLETE. NOW RECEIVING...\n");
  MESSAGE_RECEIVE_REFERENCE (reference, jarray, result);
  DecrementContextCounter ();
  return NS_OK;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::GetArrayElements (jni_type type,
                                 jarray array,
                                 jboolean* isCopy,
                                 void* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::ReleaseArrayElements (jni_type type,
                                     jarray array,
                                     void* elems,
                                     jint mode)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::GetArrayRegion (jni_type type,
                               jarray array,
                               jsize start,
                               jsize len,
                               void* buf)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::SetArrayRegion (jni_type type,
                               jarray array,
                               jsize start,
                               jsize len,
                               void* buf)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::RegisterNatives (jclass clazz,
                                JNINativeMethod const* methods,
                                jint nMethods,
                                jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::UnregisterNatives (jclass clazz,
                                  jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::MonitorEnter (jobject obj,
                             jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::MonitorExit (jobject obj,
                            jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// FIXME: never called?
// see nsJVMManager.cpp:878
// Not used.
NS_IMETHODIMP
IcedTeaJNIEnv::GetJavaVM (JavaVM** vm,
                          jint* result)
{
  NOT_IMPLEMENTED ();
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Module loading function.  See nsPluginsDirUNIX.cpp.
// FIXME: support unloading, free'ing memory NSGetFactory allocates?
extern "C" NS_EXPORT nsresult
NSGetFactory (nsISupports* aServMgr, nsCID const& aClass,
              char const* aClassName, char const* aContractID,
              nsIFactory** aFactory)
{
  static NS_DEFINE_CID (PluginCID, NS_PLUGIN_CID);
  if (!aClass.Equals (PluginCID))
    return NS_ERROR_FACTORY_NOT_LOADED;

  // Set appletviewer_executable.
  Dl_info info;
  char* filename = NULL;
  if (dladdr (reinterpret_cast<void const*> (NSGetFactory),
              &info) == 0)
    {
      PLUGIN_ERROR_TWO ("Failed to determine plugin shared object filename",
                        dlerror ());
      return NS_ERROR_FAILURE;
    }
  // Freed below.
 filename = strdup (info.dli_fname);
  if (!filename)
    {
      PLUGIN_ERROR ("Failed to create plugin shared object filename.");
      return NS_ERROR_OUT_OF_MEMORY;
    }
  nsCString executable (dirname (filename));
  nsCString jar(dirname (filename));
  nsCString extrajars("");
  free (filename);
  filename = NULL;

  //executableString += nsCString ("/../../bin/pluginappletviewer");
  executable += nsCString ("/../../bin/java");

  //executable += nsCString ("/client/libjvm.so");

  // Never freed.
  appletviewer_executable = strdup (executable.get ());
  //libjvm_so = strdup (executable.get ());
  if (!appletviewer_executable)
    {
      PLUGIN_ERROR ("Failed to create java executable name.");
      return NS_ERROR_OUT_OF_MEMORY;
    }

  if (factory_created == PR_TRUE)
  {
	  // wait for factory to initialize
	  while (!factory)
	  {
		  PR_Sleep(200);
		  PLUGIN_DEBUG("Waiting for factory to be created...");
	  }


      PLUGIN_DEBUG("NSGetFactory: Returning existing factory");

	  *aFactory = factory;
	  NS_ADDREF (factory);
  } else
  {
    factory_created = PR_TRUE;
    PLUGIN_DEBUG("NSGetFactory: Creating factory");
    factory = new IcedTeaPluginFactory ();
    if (!factory)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF (factory);
    *aFactory = factory;
  }

  return NS_OK;
}

// FIXME: replace with NS_GetCurrentThread.
PRThread*
current_thread ()
{
  nsCOMPtr<nsIComponentManager> manager;
  nsresult result = NS_GetComponentManager (getter_AddRefs (manager));
  PLUGIN_CHECK ("get component manager", result);

  nsCOMPtr<nsIThreadManager> threadManager;
  result = manager->CreateInstanceByContractID
    (NS_THREADMANAGER_CONTRACTID, nsnull, NS_GET_IID (nsIThreadManager),
     getter_AddRefs (threadManager));
  PLUGIN_CHECK ("thread manager", result);

  // threadManager is NULL during shutdown.
  if (threadManager)
    {
      nsCOMPtr<nsIThread> current;
      result = threadManager->GetCurrentThread (getter_AddRefs (current));
      //  PLUGIN_CHECK ("current thread", result);

      PRThread* threadPointer;
      result = current->GetPRThread (&threadPointer);
      //  PLUGIN_CHECK ("thread pointer", result);

      return threadPointer;
    }
  else
    return NULL;
}
