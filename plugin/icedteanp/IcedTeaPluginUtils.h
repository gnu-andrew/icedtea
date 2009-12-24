/* IcedTeaPluginUtils.h

   Copyright (C) 2009  Red Hat

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

/**
 * Utility classes for the IcedTeaPlugin
 */

#ifndef __ICEDTEAPLUGINUTILS_H__
#define __ICEDTEAPLUGINUTILS_H__

#include <pthread.h>
#include <stdio.h>

#include <cstring>
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

#include <npapi.h>

#if MOZILLA_VERSION_COLLAPSED < 1090100
#include <npupp.h>
#else
#include <npapi.h>
#include <npruntime.h>
#endif

#include "IcedTeaNPPlugin.h"

#define PLUGIN_DEBUG_0ARG(str) \
  do                                        \
  {                                         \
    if (plugin_debug)                       \
    {                                       \
      fprintf(stderr, "GCJ PLUGIN: thread %p: ", pthread_self()); \
      fprintf(stderr, str);                \
    }                                       \
  } while (0)

#define PLUGIN_DEBUG_1ARG(str, arg1) \
  do                                        \
  {                                         \
    if (plugin_debug)                       \
    {                                       \
      fprintf(stderr, "GCJ PLUGIN: thread %p: ", pthread_self()); \
      fprintf(stderr, str, arg1);          \
    }                                       \
  } while (0)

#define PLUGIN_DEBUG_2ARG(str, arg1, arg2)  \
  do                                        \
  {                                         \
    if (plugin_debug)                       \
    {                                       \
      fprintf(stderr, "GCJ PLUGIN: thread %p: ", pthread_self()); \
      fprintf(stderr, str, arg1, arg2);    \
    }                                       \
  } while (0)

#define PLUGIN_DEBUG_3ARG(str, arg1, arg2, arg3) \
  do                                           \
  {                                            \
    if (plugin_debug)                          \
    {                                          \
      fprintf(stderr, "GCJ PLUGIN: thread %p: ", pthread_self()); \
      fprintf(stderr, str, arg1, arg2, arg3); \
    }                                          \
  } while (0)

#define PLUGIN_DEBUG_4ARG(str, arg1, arg2, arg3, arg4) \
  do                                                 \
  {                                                  \
    if (plugin_debug)                                \
    {                                                \
      fprintf(stderr, "GCJ PLUGIN: thread %p: ", pthread_self()); \
      fprintf(stderr, str, arg1, arg2, arg3, arg4); \
    }                                                \
  } while (0)

#define PLUGIN_DEBUG_5ARG(str, arg1, arg2, arg3, arg4, arg5) \
  do                                                 \
  {                                                  \
    if (plugin_debug)                                \
    {                                                \
      fprintf(stderr, "GCJ PLUGIN: thread %p: ", pthread_self()); \
      fprintf(stderr, str, arg1, arg2, arg3, arg4, arg5); \
    }                                                \
  } while (0)

#define CHECK_JAVA_RESULT(result_data)                               \
{                                                                    \
    if (((JavaResultData*) result_data)->error_occurred)             \
    {                                                                \
        printf("Error: Error occurred on Java side: %s.\n",          \
               ((JavaResultData*) result_data)->error_msg->c_str()); \
        return;                                                      \
    }                                                                \
}

#define HEX_TO_INT(c) \
    ((*c >= 'A') ? *c - 'A' + 10 : \
     (*c >= 'a') ? *c - 'a' + 10 : \
     *c - '0')

#define IS_VALID_HEX(c) \
    ((*c >= '0' && *c <= '9') || \
     (*c >= 'a' && *c <= 'f') || \
     (*c >= 'A' && *c <= 'F'))

/*
 * This struct holds data specific to a Java operation requested by the plugin
 */
typedef struct java_result_data
{

    // Return identifier (if applicable)
    int return_identifier;

    // Return string (if applicable)
    std::string* return_string;

    // Return wide/mb string (if applicable)
    std::wstring* return_wstring;

    // Error message (if an error occurred)
    std::string* error_msg;

    // Boolean indicating if an error occurred
    bool error_occurred;

} JavaResultData;

/*
 * Misc. utility functions
 *
 * This class is never instantiated and should contain static functions only
 */

class IcedTeaPluginUtilities
{

    private:
        static int reference; /* Reference count */

        /* Mutex lock for updating reference count */
        static pthread_mutex_t reference_mutex;

        /* Map holding window pointer<->instance relationships */
        static std::map<void*, NPP>* instance_map;

        /* Map holding java-side-obj-key->NPObject relationship  */
        static std::map<std::string, NPObject*>* object_map;

    public:

    	/* Constructs message prefix with given context */
    	static void constructMessagePrefix(int context,
                                           std::string* result);

    	/* Constructs message prefix with given context and reference */
    	static void constructMessagePrefix(int context, int reference,
                                           std::string* result);

    	/* Constructs message prefix with given context, reference and src */
    	static void constructMessagePrefix(int context, int reference,
                                           std::string address,
                                           std::string* result);

    	/* Converts given pointer to a string representation */
    	static void JSIDToString(void* id, std::string* result);

    	/* Converts the given string representation to a pointer */
    	static void* stringToJSID(std::string id_str);

    	/* Increments reference count and returns it */
    	static int getReference();

    	/* Decrements reference count */
    	static void releaseReference();

    	/* Converts the given integer to a string */
    	static void itoa(int i, std::string* result);

    	/* Frees the given vector and the strings that its contents point to */
    	static void freeStringPtrVector(std::vector<std::string*>* v);

    	/* Splits the given string based on the delimiter provided */
    	static std::vector<std::string>* strSplit(const char* str,
    			                                  const char* delim);

    	/* Converts given unicode integer byte array to UTF8 string  */
    	static void getUTF8String(int length, int begin,
    			std::vector<std::string>* unicode_byte_array,
    			std::string* result_unicode_str);

    	/* Converts given UTF8 string to unicode integer byte array */
    	static void convertStringToUTF8(std::string* str,
    			                        std::string* utf_str);

    	/* Converts given unicode integer byte array to UTF16LE/UCS-2 string */
    	static void getUTF16LEString(int length, int begin,
    			std::vector<std::string>* unicode_byte_array,
    			std::wstring* result_unicode_str);

    	/* Prints contents of given string vector */
    	static void printStringVector(const char* prefix, std::vector<std::string>* cv);

    	/* Prints contents of given string pointer vector */
    	static void printStringPtrVector(const char* prefix, std::vector<std::string*>* cv);

    	static std::string* variantToClassName(NPVariant variant);

    	static void printNPVariant(NPVariant variant);

    	static void NPVariantToString(NPVariant variant, std::string* result);

        static bool javaResultToNPVariant(NPP instance,
                                          std::string* java_result,
                                          NPVariant* variant);

    	static const gchar* getSourceFromInstance(NPP instance);

    	static void storeInstanceID(void* member_ptr, NPP instance);

    	static void	removeInstanceID(void* member_ptr);

        static NPP getInstanceFromMemberPtr(void* member_ptr);

    	static NPObject* getNPObjectFromJavaKey(std::string key);

    	static void storeObjectMapping(std::string key, NPObject* object);

    	static void removeObjectMapping(std::string key);

    	static void invalidateInstance(NPP instance);

    	static bool isObjectJSArray(NPP instance, NPObject* object);

    	static void decodeURL(const char* url, char** decoded_url);
};

/*
 * A bus subscriber interface. Implementors must implement the newMessageOnBus
 * method.
 */
class BusSubscriber
{
    private:

    public:
    	BusSubscriber() {}

    	/* Notifies this subscriber that a new message as arrived */
        virtual bool newMessageOnBus(const char* message) = 0;
};

/*
 * This implementation is very simple and is therefore folded into this file
 * rather than a new one.
 */
class JavaMessageSender : public BusSubscriber
{
    private:
    public:

    	/* Sends given message to Java side */
        virtual bool newMessageOnBus(const char* message);
};

/*
 * Represents a message bus.
 * The bus can also have subscribers who are notified when a new message
 * arrives.
 */
class MessageBus
{
    private:
    	/* Mutex for locking the message queue */
    	pthread_mutex_t msg_queue_mutex;

    	/* Mutex used when adjusting subscriber list */
    	pthread_mutex_t subscriber_mutex;

    	/* Subscriber list */
        std::list<BusSubscriber*> subscribers;

        /* Queued messages */
        std::queue<char*> msgQueue;

    public:
    	MessageBus();

        ~MessageBus();

        /* subscribe to this bus */
        void subscribe(BusSubscriber* b);

        /* unsubscribe from this bus */
        void unSubscribe(BusSubscriber* b);

        /* Post a message on to the bus (it is safe to free the message pointer
           after this function returns) */
        void post(const char* message);
};

#endif // __ICEDTEAPLUGINUTILS_H__
