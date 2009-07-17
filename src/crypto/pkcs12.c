/*
 * pkcs12.c
 *
 * Copyright (C) AB Strakt 2001, All rights reserved
 *
 * Certificate transport (PKCS12) handling code, 
 * mostly thin wrappers around OpenSSL.
 * See the file RATIONALE for a short explanation of why 
 * this module was written.
 *
 * Reviewed 2001-07-23
 */
#include <Python.h>
#define crypto_MODULE
#include "crypto.h"

/* 
 * PKCS12 is a standard exchange format for digital certificates.  
 * See e.g. the OpenSSL homepage http://www.openssl.org/ for more information
 */

static void crypto_PKCS12_dealloc(crypto_PKCS12Obj *self);

static char crypto_PKCS12_get_certificate_doc[] = "\n\
Return certificate portion of the PKCS12 structure\n\
\n\
@return: X509 object containing the certificate\n\
";
static PyObject *
crypto_PKCS12_get_certificate(crypto_PKCS12Obj *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_certificate"))
        return NULL;

    Py_INCREF(self->cert);
    return self->cert;
}

static char crypto_PKCS12_set_certificate_doc[] = "\n\
Replace or set the certificate portion of the PKCS12 structure\n\
\n\
Arguments: self - The PKCS12 object\n\
           args - The Python argument tuple \n\
             cert - The new certificate\n\
Returns:   self\n\
";
static crypto_PKCS12Obj *
crypto_PKCS12_set_certificate(crypto_PKCS12Obj *self, PyObject *args, PyObject *keywds)
{
    PyObject *cert = NULL;
    static char *kwlist[] = {"cert", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O:set_certificate", 
        kwlist, &cert))
        return NULL;

    if (cert != Py_None && ! PyObject_IsInstance(cert, (PyObject *) &crypto_X509_Type)) { 
        PyErr_SetString(PyExc_TypeError, "cert must be type X509 or None");
        return NULL;
    }

    Py_INCREF(cert);  /* Make consistent before calling Py_DECREF() */
    if(self->cert) {
        Py_DECREF(self->cert);
    }
    self->cert = cert;

    Py_INCREF(self);
    return self;
}

static char crypto_PKCS12_get_privatekey_doc[] = "\n\
Return private key portion of the PKCS12 structure\n\
\n\
@returns: PKey object containing the private key\n\
";
//static PyObject *
static crypto_PKeyObj *
crypto_PKCS12_get_privatekey(crypto_PKCS12Obj *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_privatekey"))
        return NULL;

    Py_INCREF(self->key);
    return (PyObject *) self->key;
}

static char crypto_PKCS12_set_privatekey_doc[] = "\n\
Replace or set the privatekey portion of the PKCS12 structure\n\
\n\
Arguments: self - The PKCS12 object\n\
           args - The Python argument tuple \n\
             pkey - The new private key\n\
Returns:   self\n\
";
static crypto_PKCS12Obj *
crypto_PKCS12_set_privatekey(crypto_PKCS12Obj *self, PyObject *args, PyObject *keywds)
{
    PyObject *pkey = NULL;
    static char *kwlist[] = {"pkey", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O:set_privatekey", 
        kwlist, &pkey))
        return NULL;

    if (pkey != Py_None && ! PyObject_IsInstance(pkey, (PyObject *) &crypto_PKey_Type)) { 
        PyErr_SetString(PyExc_TypeError, "pkey must be type X509 or None");
        return NULL;
    }

    Py_INCREF(pkey);  /* Make consistent before calling Py_DECREF() */
    if(self->key) {
        Py_DECREF(self->key);
    }
    self->key = pkey;

    Py_INCREF(self);
    return self;
}

static char crypto_PKCS12_get_ca_certificates_doc[] = "\n\
Return CA certificates within of the PKCS12 object\n\
\n\
@return: A newly created tuple containing the CA certificates in the chain,\n\
         if any are present, or None if no CA certificates are present.\n\
";
static PyObject *
crypto_PKCS12_get_ca_certificates(crypto_PKCS12Obj *self, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ":get_ca_certificates"))
        return NULL;

    Py_INCREF(self->cacerts);
    return self->cacerts;
}

static char crypto_PKCS12_set_ca_certificates_doc[] = "\n\
Replace or set the ca_certificates portion of the PKCS12 structure\n\
\n\
Arguments: self - The PKCS12 object\n\
           args - The Python argument tuple \n\
             cacerts - The new ca_certificates\n\
Returns:   self\n\
";
static crypto_PKCS12Obj *
crypto_PKCS12_set_ca_certificates(crypto_PKCS12Obj *self, PyObject *args, PyObject *keywds)
{
    PyObject *cacerts;
    static char *kwlist[] = {"cacerts", NULL};
    int i;

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "O:set_ca_certificates", 
        kwlist, &cacerts))
        return NULL;
    if (cacerts == Py_None) {
        /* We are good. */
    } else if (PySequence_Check(cacerts)) {  /* is iterable */
        for(i = 0;i < PySequence_Length(cacerts);i++) {  /* For each CA cert */
            PyObject *obj;
            obj = PySequence_GetItem(cacerts, i);
            if (PyObject_Type(obj) != (PyObject *) &crypto_X509_Type) {
                Py_DECREF(obj);
                PyErr_SetString(PyExc_TypeError, "cacerts iterable must only contain X509Type");
                return NULL;
            }
            Py_DECREF(obj);
        }
    } else {
        PyErr_SetString(PyExc_TypeError, "cacerts must be an iterable or None");
        return NULL;
    }

    Py_INCREF(cacerts); /* Make consistent before calling Py_DECREF() */
    if(self->cacerts) {
        Py_DECREF(self->cacerts);
    }
    self->cacerts = cacerts;

    Py_INCREF(self);
    return self;
}

static char crypto_PKCS12_export_doc[] = "\n\
Dump a PKCS12 object to a buffer string\n\
\n\
Arguments: self - The PKCS12 object\n\
           args - The Python argument tuple, should be:\n\
             passphrase - (optional) for encrypting the PKCS12 string\n\
                           using 3DES-CBC\n\
             friendly_name - stored in the file for display\n\
             iter - number of iterations to use when encrypting\n\
             maciter - number of iterations to use when creating the MAC.\n\
                       A special value of -1 means no MAC.\n\
Returns:   The buffer with the dumped pkcs12 in it\n\
";

static PyObject *
crypto_PKCS12_export(crypto_PKCS12Obj *self, PyObject *args, PyObject *keywds)
{
    int buf_len;
    PyObject *buffer;
    char *temp, *passphrase = NULL, *friendly_name = NULL;
    BIO *bio;
    PKCS12 *p12;
    EVP_PKEY *pkey = NULL; 
    STACK_OF(X509) *cacerts = NULL;
    X509 *x509 = NULL;
    int iter = PKCS12_DEFAULT_ITER;
    int maciter = 0; 
    static char *kwlist[] = {"passphrase", "friendly_name", "iter", "maciter", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "|zzii:export", 
        kwlist, &passphrase, &friendly_name, &iter, &maciter))
        return NULL;

    if (self->key && self->key != Py_None) {
        pkey = ((crypto_PKeyObj*) self->key)->pkey;
    }
    if (self->cert && self->cert != Py_None) {
        x509 = ((crypto_X509Obj*) self->cert)->x509;
    }
    cacerts = sk_X509_new_null();
    if (self->cacerts && self->cacerts != Py_None) {
        int i;
        PyObject *obj;
        for(i = 0;i < PySequence_Length(self->cacerts);i++) {  /* For each CA cert */
            obj = PySequence_GetItem(self->cacerts, i);
            /* assert(PyObject_IsInstance(obj, (PyObject *) &crypto_X509_Type )); */
            sk_X509_push(cacerts, (( crypto_X509Obj* ) obj)->x509);
            Py_DECREF(obj);
        }
    }

    p12 = PKCS12_create(passphrase, friendly_name, pkey, x509, cacerts, 
                        NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                        NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
                        iter, maciter, 0);
    if( p12 == NULL ) {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }
    bio = BIO_new(BIO_s_mem());
    i2d_PKCS12_bio(bio, p12);
    buf_len = BIO_get_mem_data(bio, &temp);
    buffer = PyString_FromStringAndSize(temp, buf_len);
    BIO_free(bio);
    return buffer;
}

/*
 * ADD_METHOD(name) expands to a correct PyMethodDef declaration
 *   {  'name', (PyCFunction)crypto_PKCS12_name, METH_VARARGS, crypto_PKCS12_name_doc }
 * for convenience
 */
#define ADD_METHOD(name)        \
    { #name, (PyCFunction)crypto_PKCS12_##name, METH_VARARGS, crypto_PKCS12_##name##_doc }
#define ADD_KW_METHOD(name)        \
    { #name, (PyCFunction)crypto_PKCS12_##name, METH_VARARGS | METH_KEYWORDS, crypto_PKCS12_##name##_doc }
static PyMethodDef crypto_PKCS12_methods[] =
{
    ADD_METHOD(get_certificate),
    ADD_KW_METHOD(set_certificate),
    ADD_METHOD(get_privatekey),
    ADD_KW_METHOD(set_privatekey),
    ADD_METHOD(get_ca_certificates),
    ADD_KW_METHOD(set_ca_certificates),
    ADD_KW_METHOD(export),
    { NULL, NULL }
};
#undef ADD_METHOD

/*
 * Constructor for PKCS12 objects, never called by Python code directly.
 * The strategy for this object is to create all the Python objects
 * corresponding to the cert/key/CA certs right away
 *
 * Arguments: p12        - A "real" PKCS12 object
 *            passphrase - Passphrase to use when decrypting the PKCS12 object
 * Returns:   The newly created PKCS12 object
 */
crypto_PKCS12Obj *
crypto_PKCS12_New(PKCS12 *p12, char *passphrase)
{
    crypto_PKCS12Obj *self;
    PyObject *cacertobj = NULL;

    X509 *cert = NULL;
    EVP_PKEY *pkey = NULL;
    STACK_OF(X509) *cacerts = NULL;

    int i, cacert_count = 0;

    /* allocate space for the CA cert stack */
    cacerts = sk_X509_new_null();

    /* parse the PKCS12 lump */
    if (p12 && !(cacerts && PKCS12_parse(p12, passphrase, &pkey, &cert, &cacerts)))
    {
        exception_from_error_queue(crypto_Error);
        return NULL;
    }

    if (!(self = PyObject_GC_New(crypto_PKCS12Obj, &crypto_PKCS12_Type)))
        return NULL;

    self->cert = NULL;
    self->key = NULL;
    Py_INCREF(Py_None);
    self->cacerts = Py_None;

    if (cert == NULL) {
        Py_INCREF(Py_None);
        self->cert = Py_None;
    } else {
        if ((self->cert = (PyObject *)crypto_X509_New(cert, 1)) == NULL)
            goto error;
    } 
    if (pkey == NULL) {
        Py_INCREF(Py_None);
        self->key = Py_None;
    } else {
        if ((self->key = (PyObject *)crypto_PKey_New(pkey, 1)) == NULL)
            goto error;
    }

    /* Make a tuple for the CA certs */
    cacert_count = sk_X509_num(cacerts);
    if (cacert_count > 0)
    {
        Py_DECREF(self->cacerts);
        if ((self->cacerts = PyTuple_New(cacert_count)) == NULL)
            goto error;

        for (i = 0; i < cacert_count; i++)
        {
            cert = sk_X509_value(cacerts, i);
            if ((cacertobj = (PyObject *)crypto_X509_New(cert, 1)) == NULL)
                goto error;
            PyTuple_SET_ITEM(self->cacerts, i, cacertobj);
        }
    }

    sk_X509_free(cacerts); /* don't free the certs, just the stack */
    PyObject_GC_Track(self);

    return self;
error:
    crypto_PKCS12_dealloc(self);
    return NULL;
}

/*
 * Find attribute
 *
 * Arguments: self - The PKCS12 object
 *            name - The attribute name
 * Returns:   A Python object for the attribute, or NULL if something went
 *            wrong
 */
static PyObject *
crypto_PKCS12_getattr(crypto_PKCS12Obj *self, char *name)
{
    return Py_FindMethod(crypto_PKCS12_methods, (PyObject *)self, name);
}

/*
 * Call the visitproc on all contained objects.
 *
 * Arguments: self - The PKCS12 object
 *            visit - Function to call
 *            arg - Extra argument to visit
 * Returns:   0 if all goes well, otherwise the return code from the first
 *            call that gave non-zero result.
 */
static int
crypto_PKCS12_traverse(crypto_PKCS12Obj *self, visitproc visit, void *arg)
{
    int ret = 0;

    if (ret == 0 && self->cert != NULL)
        ret = visit(self->cert, arg);
    if (ret == 0 && self->key != NULL)
        ret = visit(self->key, arg);
    if (ret == 0 && self->cacerts != NULL)
        ret = visit(self->cacerts, arg);
    return ret;
}

/*
 * Decref all contained objects and zero the pointers.
 *
 * Arguments: self - The PKCS12 object
 * Returns:   Always 0.
 */
static int
crypto_PKCS12_clear(crypto_PKCS12Obj *self)
{
    Py_XDECREF(self->cert);
    self->cert = NULL;
    Py_XDECREF(self->key);
    self->key = NULL;
    Py_XDECREF(self->cacerts);
    self->cacerts = NULL;
    return 0;
}

/*
 * Deallocate the memory used by the PKCS12 object
 *
 * Arguments: self - The PKCS12 object
 * Returns:   None
 */
static void
crypto_PKCS12_dealloc(crypto_PKCS12Obj *self)
{
    PyObject_GC_UnTrack(self);
    crypto_PKCS12_clear(self);
    PyObject_GC_Del(self);
}

PyTypeObject crypto_PKCS12_Type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "PKCS12",
    sizeof(crypto_PKCS12Obj),
    0,
    (destructor)crypto_PKCS12_dealloc,
    NULL, /* print */
    (getattrfunc)crypto_PKCS12_getattr,
    NULL, /* setattr */
    NULL, /* compare */
    NULL, /* repr */
    NULL, /* as_number */
    NULL, /* as_sequence */
    NULL, /* as_mapping */
    NULL, /* hash */
    NULL, /* call */
    NULL, /* str */
    NULL, /* getattro */
    NULL, /* setattro */
    NULL, /* as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    NULL, /* doc */
    (traverseproc)crypto_PKCS12_traverse,
    (inquiry)crypto_PKCS12_clear,
};

/*
 * Initialize the PKCS12 part of the crypto sub module
 *
 * Arguments: module - The crypto module
 * Returns:   None
 */
int
init_crypto_pkcs12(PyObject *module) {
    if (PyType_Ready(&crypto_PKCS12_Type) < 0) {
        return 0;
    }

    if (PyModule_AddObject(module, "PKCS12Type", (PyObject *)&crypto_PKCS12_Type) != 0) {
        return 0;
    }

    return 1;
}

