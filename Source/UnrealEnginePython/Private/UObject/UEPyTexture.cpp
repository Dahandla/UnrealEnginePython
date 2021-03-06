#include "UnrealEnginePythonPrivatePCH.h"

#include "Runtime/Engine/Public/ImageUtils.h"
#include "Runtime/Engine/Classes/Engine/Texture.h"

PyObject *py_ue_texture_get_width(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	UTexture2D *texture = ue_py_check_type<UTexture2D>(self);
	if (!texture)
		return PyErr_Format(PyExc_Exception, "object is not a Texture");

	return PyLong_FromLong(texture->GetSizeX());
}

PyObject *py_ue_texture_get_height(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	UTexture2D *texture = ue_py_check_type<UTexture2D>(self);
	if (!texture)
		return PyErr_Format(PyExc_Exception, "object is not a Texture");

	return PyLong_FromLong(texture->GetSizeY());
}

PyObject *py_ue_texture_get_data(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	int mipmap = 0;

	if (!PyArg_ParseTuple(args, "|i:texture_get_data", &mipmap)) {
		return NULL;
	}

	UTexture2D *tex = ue_py_check_type<UTexture2D>(self);
	if (!tex)
		return PyErr_Format(PyExc_Exception, "object is not a Texture2D");

	if (mipmap >= tex->GetNumMips())
		return PyErr_Format(PyExc_Exception, "invalid mipmap id");

	const char *blob = (const char*)tex->PlatformData->Mips[mipmap].BulkData.Lock(LOCK_READ_ONLY);
	PyObject *bytes = PyByteArray_FromStringAndSize(blob, (Py_ssize_t)tex->PlatformData->Mips[mipmap].BulkData.GetBulkDataSize());
	tex->PlatformData->Mips[mipmap].BulkData.Unlock();
	return bytes;
}

PyObject *py_ue_render_target_get_data(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	int mipmap = 0;

	if (!PyArg_ParseTuple(args, "|i:render_target_get_data", &mipmap)) {
		return NULL;
	}

	UTextureRenderTarget2D *tex = ue_py_check_type<UTextureRenderTarget2D>(self);
	if (!tex)
		return PyErr_Format(PyExc_Exception, "object is not a TextureRenderTarget");

	FTextureRenderTarget2DResource *resource = (FTextureRenderTarget2DResource *)tex->Resource;
	if (!resource) {
		return PyErr_Format(PyExc_Exception, "cannot get render target resource");
	}

	TArray<FColor> pixels;
	if (!resource->ReadPixels(pixels)) {
		return PyErr_Format(PyExc_Exception, "unable to read pixels");
	}

	return PyByteArray_FromStringAndSize((const char *)pixels.GetData(), (Py_ssize_t)(tex->GetSurfaceWidth() * 4 * tex->GetSurfaceHeight()));
}

PyObject *py_ue_texture_set_data(ue_PyUObject *self, PyObject * args) {

	ue_py_check(self);

	Py_buffer py_buf;
	int mipmap = 0;

	if (!PyArg_ParseTuple(args, "z*|i:texture_set_data", &py_buf, &mipmap)) {
		return NULL;
	}

	UTexture2D *tex = ue_py_check_type<UTexture2D>(self);
	if (!tex)
		return PyErr_Format(PyExc_Exception, "object is not a Texture2D");


	if (!py_buf.buf)
		return PyErr_Format(PyExc_Exception, "invalid data");

	if (mipmap >= tex->GetNumMips())
		return PyErr_Format(PyExc_Exception, "invalid mipmap id");

	char *blob = (char*)tex->PlatformData->Mips[mipmap].BulkData.Lock(LOCK_READ_WRITE);
	int32 len = tex->PlatformData->Mips[mipmap].BulkData.GetBulkDataSize();
	int32 wanted_len = py_buf.len;
	// avoid making mess
	if (wanted_len > len) {
		UE_LOG(LogPython, Warning, TEXT("truncating buffer to %d bytes"), len);
		wanted_len = len;
	}
	FMemory::Memcpy(blob, py_buf.buf, wanted_len);
	tex->PlatformData->Mips[mipmap].BulkData.Unlock();

	tex->MarkPackageDirty();
#if WITH_EDITOR
	tex->PostEditChange();
#endif

	tex->UpdateResource();

	Py_INCREF(Py_None);
	return Py_None;
}

PyObject *py_unreal_engine_compress_image_array(PyObject * self, PyObject * args) {
	int width;
	int height;
	Py_buffer py_buf;
	if (!PyArg_ParseTuple(args, "iiz*:compress_image_array", &width, &height, &py_buf)) {
		return NULL;
	}

	if (py_buf.buf == nullptr || py_buf.len <= 0) {
		PyBuffer_Release(&py_buf);
		return PyErr_Format(PyExc_Exception, "invalid image data");
	}

	TArray<FColor> colors;
	uint8 *buf = (uint8 *)py_buf.buf;
	for (int32 i = 0; i < py_buf.len; i += 4) {
		colors.Add(FColor(buf[i], buf[1 + 1], buf[i + 2], buf[i + 3]));
	}

	TArray<uint8> output;

	FImageUtils::CompressImageArray(width, height, colors, output);

	return PyBytes_FromStringAndSize((char *)output.GetData(), output.Num());
}

PyObject *py_unreal_engine_create_checkerboard_texture(PyObject * self, PyObject * args) {
	PyObject *py_color_one;
	PyObject *py_color_two;
	int checker_size;
	if (!PyArg_ParseTuple(args, "OOi:create_checkboard_texture", &py_color_one, &py_color_two, &checker_size)) {
		return NULL;
	}

	ue_PyFColor *color_one = py_ue_is_fcolor(py_color_one);
	if (!color_one)
		return PyErr_Format(PyExc_Exception, "argument is not a FColor");

	ue_PyFColor *color_two = py_ue_is_fcolor(py_color_two);
	if (!color_two)
		return PyErr_Format(PyExc_Exception, "argument is not a FColor");

	UTexture2D *texture = FImageUtils::CreateCheckerboardTexture(color_one->color, color_two->color, checker_size);

	ue_PyUObject *ret = ue_get_python_wrapper(texture);
	if (!ret)
		return PyErr_Format(PyExc_Exception, "uobject is in invalid state");
	Py_INCREF(ret);
	return (PyObject *)ret;
}

PyObject *py_unreal_engine_create_transient_texture(PyObject * self, PyObject * args) {
	int width;
	int height;
	int format = PF_B8G8R8A8;
	if (!PyArg_ParseTuple(args, "ii|i:create_transient_texture", &width, &height, &format)) {
		return NULL;
	}


	UTexture2D *texture = UTexture2D::CreateTransient(width, height, (EPixelFormat)format);
	if (!texture)
		return PyErr_Format(PyExc_Exception, "unable to create texture");

	texture->UpdateResource();

	ue_PyUObject *ret = ue_get_python_wrapper(texture);
	if (!ret)
		return PyErr_Format(PyExc_Exception, "uobject is in invalid state");
	Py_INCREF(ret);
	return (PyObject *)ret;
}

