/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "../../src/stdint.hpp"
#include "../c/zmq.h"

#include "org_zmq_Socket.h"

static jfieldID socket_handle_fid = NULL;
static jmethodID create_socket_mid = NULL;

static void raise_exception (JNIEnv *env, int err)
{
    //  Get exception class.
    jclass exception_class = env->FindClass ("java/lang/Exception");
    assert (exception_class);

    //  Get text description of the exception.
    const char *err_msg = zmq_strerror (err);

    //  Raise the exception.
    int rc = env->ThrowNew (exception_class, err_msg);
    assert (rc == 0);

    //  Free the local ref.
    env->DeleteLocalRef (exception_class);
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_construct (JNIEnv *env, jobject obj,
    jobject context, jint type)
{
    if (socket_handle_fid == NULL) {
        jclass cls = env->GetObjectClass (obj);
        assert (cls);
        socket_handle_fid = env->GetFieldID (cls, "socketHandle", "J");
        assert (socket_handle_fid);
        env->DeleteLocalRef (cls);
    }

    if (create_socket_mid == NULL) {
        jclass cls = env->FindClass ("org/zmq/Context");
        assert (cls);
        create_socket_mid = env->GetMethodID (cls, "createSocket", "(I)J");
        assert (create_socket_mid);
        env->DeleteLocalRef (cls);
    }

    void *s = (void*) env->CallLongMethod (context, create_socket_mid, type);
    if (env->ExceptionCheck ())
        return;

    env->SetLongField (obj, socket_handle_fid, (jlong) s);
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_finalize (JNIEnv *env, jobject obj)
{
    void *s = (void*) env->GetLongField (obj, socket_handle_fid);
    assert (s);
    int rc = zmq_close (s);
    assert (rc == 0);
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_setsockopt__IJ (JNIEnv *env,
    jobject obj, jint option, jlong optval)
{
    switch (option) {
    case ZMQ_HWM:
    case ZMQ_LWM:
    case ZMQ_SWAP:
    case ZMQ_AFFINITY:
    case ZMQ_RATE:
    case ZMQ_RECOVERY_IVL:
    case ZMQ_MCAST_LOOP:
        {
            void *s = (void*) env->GetLongField (obj, socket_handle_fid);
            assert (s);

            int64_t value = optval;
            int rc = zmq_setsockopt (s, option, &value, sizeof (value));
            if (rc != 0)
                raise_exception (env, errno);
            return;
        }
    default:
        raise_exception (env, EINVAL);
        return;
    }
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_setsockopt__ILjava_lang_String_2 (
    JNIEnv *env, jobject obj, jint option, jstring optval)
{
    switch (option) {
    case ZMQ_IDENTITY:
    case ZMQ_SUBSCRIBE:
    case ZMQ_UNSUBSCRIBE:
        {
            if (optval == NULL) {
                raise_exception (env, EINVAL);
                return;
            }

            void *s = (void*) env->GetLongField (obj, socket_handle_fid);
            assert (s);

            const char *value = env->GetStringUTFChars (optval, NULL);
            assert (value);
            int rc = zmq_setsockopt (s, option, value, strlen (value));
            env->ReleaseStringUTFChars (optval, value);
            if (rc != 0)
                raise_exception (env, errno);
            return;
        }
    default:
        raise_exception (env, EINVAL);
        return;
    }
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_bind (JNIEnv *env, jobject obj,
    jstring addr)
{
    void *s = (void*) env->GetLongField (obj, socket_handle_fid);
    assert (s);

    if (addr == NULL) {
        raise_exception (env, EINVAL);
        return;
    }

    const char *c_addr = env->GetStringUTFChars (addr, NULL);
    if (c_addr == NULL) {
        raise_exception (env, EINVAL);
        return;
    }

    int rc = zmq_bind (s, c_addr);
    env->ReleaseStringUTFChars (addr, c_addr);

    if (rc == -1)
        raise_exception (env, errno);
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_connect (JNIEnv *env, jobject obj,
    jstring addr)
{
    void *s = (void*) env->GetLongField (obj, socket_handle_fid);
    assert (s);

    if (addr == NULL) {
        raise_exception (env, EINVAL);
        return;
    }

    const char *c_addr = env->GetStringUTFChars (addr, NULL);
    if (c_addr == NULL) {
        raise_exception (env, EINVAL);
        return;
    }

    int rc = zmq_connect (s, c_addr);
    env->ReleaseStringUTFChars (addr, c_addr);

    if (rc == -1)
        raise_exception (env, errno);
}

JNIEXPORT jboolean JNICALL Java_org_zmq_Socket_send (JNIEnv *env, jobject obj,
    jbyteArray msg, jlong flags)
{
    void *s = (void*) env->GetLongField (obj, socket_handle_fid);
    assert (s);

    jsize size = env->GetArrayLength (msg); 
    jbyte *data = env->GetByteArrayElements (msg, 0);

    zmq_msg_t message;
    int rc = zmq_msg_init_size (&message, size);
    assert (rc == 0);
    memcpy (zmq_msg_data (&message), data, size);

    env->ReleaseByteArrayElements (msg, data, 0);

    rc = zmq_send (s, &message, (int) flags);
        
    if (rc == -1 && errno == EAGAIN) {
        rc = zmq_msg_close (&message);
        assert (rc == 0);
        return JNI_FALSE;
    }
    
    if (rc == -1) {
        raise_exception (env, errno);
        rc = zmq_msg_close (&message);
        assert (rc == 0);
        return JNI_FALSE;
    }

    rc = zmq_msg_close (&message);
    assert (rc == 0);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_org_zmq_Socket_flush (JNIEnv *env, jobject obj)
{
    void *s = (void*) env->GetLongField (obj, socket_handle_fid);
    assert (s);

    int rc = zmq_flush (s);

    if (rc == -1) {
        raise_exception (env, errno);
        return ;
    }
}

JNIEXPORT jbyteArray JNICALL Java_org_zmq_Socket_recv (JNIEnv *env, jobject obj,
    jlong flags)
{
    void *s = (void*) env->GetLongField (obj, socket_handle_fid);
    assert (s);

    zmq_msg_t message;
    zmq_msg_init (&message);
    int rc = zmq_recv (s, &message, (int) flags);

    if (rc == -1 && errno == EAGAIN) {
        zmq_msg_close (&message);
        return NULL;
    }

    if (rc == -1) {
        raise_exception (env, errno);
        zmq_msg_close (&message);
        return NULL;
    }

    jbyteArray data = env->NewByteArray (zmq_msg_size (&message));
    assert (data);
    env->SetByteArrayRegion (data, 0, zmq_msg_size (&message),
        (jbyte*) zmq_msg_data (&message));

    return data;
}
