//--------------------------------------------------------------------------

static PyObject* wxPyGetMethod(PyObject* py, char* name)
{
    if (!PyObject_HasAttrString(py, name))
        return NULL;
    PyObject* o = PyObject_GetAttrString(py, name);
    if (!PyMethod_Check(o) && !PyCFunction_Check(o)) {
        Py_DECREF(o);
        return NULL;
    }
    return o;
}

#define wxPyBlock_t_default PyGILState_UNLOCKED


// This class can wrap a Python file-like object and allow it to be used 
// as a wxInputStream.
class wxPyInputStream : public wxInputStream
{
public:

    // Make sure there is at least a read method
    static bool Check(PyObject* fileObj) 
    {
        PyObject* method = wxPyGetMethod(fileObj, "read");
        bool rval = method != NULL;
        Py_XDECREF(method);
        return rval;
    }

    wxPyInputStream(PyObject* fileObj, bool block=true) 
    {
        m_block = block;
        wxPyBlock_t blocked = wxPyBlock_t_default;
        if (block) blocked = wxPyBeginBlockThreads();
    
        m_read = wxPyGetMethod(fileObj, "read");
        m_seek = wxPyGetMethod(fileObj, "seek");
        m_tell = wxPyGetMethod(fileObj, "tell");
    
        if (block) wxPyEndBlockThreads(blocked);
    }
    
    virtual ~wxPyInputStream()
    {
        wxPyBlock_t blocked = wxPyBlock_t_default;
        if (m_block) blocked = wxPyBeginBlockThreads();
        Py_XDECREF(m_read);
        Py_XDECREF(m_seek);
        Py_XDECREF(m_tell);
        if (m_block) wxPyEndBlockThreads(blocked);
    }
    
    wxPyInputStream(const wxPyInputStream& other)
    {
        wxPyBlock_t blocked = wxPyBeginBlockThreads();
        m_read  = other.m_read;
        m_seek  = other.m_seek;
        m_tell  = other.m_tell;
        m_block = other.m_block;
        Py_INCREF(m_read);
        Py_INCREF(m_seek);
        Py_INCREF(m_tell);
        wxPyEndBlockThreads(blocked);
    }
    
protected:

    // implement base class virtuals
    
    wxFileOffset GetLength() const 
    {
        wxPyInputStream* self = (wxPyInputStream*)this; // cast off const
        if (m_seek && m_tell) {
            wxFileOffset temp = self->OnSysTell();
            wxFileOffset ret = self->OnSysSeek(0, wxFromEnd);
            self->OnSysSeek(temp, wxFromStart);
            return ret;
        }
        else
            return wxInvalidOffset;
    }

    size_t OnSysRead(void *buffer, size_t bufsize) 
    {
        if (bufsize == 0)
            return 0;
    
        wxPyBlock_t blocked = wxPyBeginBlockThreads();
        PyObject* arglist = Py_BuildValue("(i)", bufsize);
        PyObject* result = PyEval_CallObject(m_read, arglist);
        Py_DECREF(arglist);
    
        size_t o = 0;
        if ((result != NULL) && PyString_Check(result)) {
            o = PyString_Size(result);
            if (o == 0)
                m_lasterror = wxSTREAM_EOF;
            if (o > bufsize)
                o = bufsize;
            memcpy((char*)buffer, PyString_AsString(result), o);  // strings only, not unicode...
            Py_DECREF(result);
    
        }
        else
            m_lasterror = wxSTREAM_READ_ERROR;
        wxPyEndBlockThreads(blocked);
        return o;
    }
    
    size_t OnSysWrite(const void *buffer, size_t bufsize) 
    {
        m_lasterror = wxSTREAM_WRITE_ERROR;
        return 0;
    }
    
    wxFileOffset OnSysSeek(wxFileOffset off, wxSeekMode mode) 
    {
        wxPyBlock_t blocked = wxPyBeginBlockThreads();
        PyObject* arglist = PyTuple_New(2);
    
        if (sizeof(wxFileOffset) > sizeof(long))
            // wxFileOffset is a 64-bit value...
            PyTuple_SET_ITEM(arglist, 0, PyLong_FromLongLong(off));
        else
            PyTuple_SET_ITEM(arglist, 0, PyInt_FromLong(off));
    
        PyTuple_SET_ITEM(arglist, 1, PyInt_FromLong(mode));
    
    
        PyObject* result = PyEval_CallObject(m_seek, arglist);
        Py_DECREF(arglist);
        Py_XDECREF(result);
        wxPyEndBlockThreads(blocked);
        return OnSysTell();
    }
    
    wxFileOffset OnSysTell() const 
    {
        wxPyBlock_t blocked = wxPyBeginBlockThreads();
        PyObject* arglist = Py_BuildValue("()");
        PyObject* result = PyEval_CallObject(m_tell, arglist);
        Py_DECREF(arglist);
        wxFileOffset o = 0;
        if (result != NULL) {
            if (PyLong_Check(result))
                o = PyLong_AsLongLong(result);
            else
                o = PyInt_AsLong(result);
            Py_DECREF(result);
        };
        wxPyEndBlockThreads(blocked);
        return o;
    }
        
    bool IsSeekable() const 
    {
        return (m_seek != NULL);
    }

    
private:
    PyObject* m_read;
    PyObject* m_seek;
    PyObject* m_tell;
    bool      m_block;
};
        
//--------------------------------------------------------------------------