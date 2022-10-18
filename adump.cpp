/**
 * A very simple BMP 2 ASCII.
 * This programm can only read uncompressed 32bit and 24bit bitmaps.
 * - 
 * october 2022
 * Marcio Freitas (marciovmf) 
 */
#include <stdio.h>
#include <math.h>
#include <string.h>

static const int BITMAP_SIGNATURE = 0x4D42; // 'BM'
static const int BITMAP_COMPRESSION_BI_BITFIELDS = 3;

#pragma pack(push, 1)
struct BitmapHeader {
	unsigned short  type;
	unsigned int    bitmapSize;
	unsigned short  reserved1;
	unsigned short  reserved2;
	unsigned int    offBits;
	unsigned int    size;
	unsigned int    width;
	unsigned int    height;
	unsigned short  planes;
	unsigned short  bitCount;
	unsigned int    compression;
	unsigned int    sizeImage;
	unsigned int    xPelsPerMeter;
	unsigned int    yPelsPerMeter;
	unsigned int    clrUsed;
	unsigned int    clrImportant;
};
#pragma pack(pop)

struct Palette
{
	int count;
	const char* chars;
};

struct Image
{
	int width;
	int height;
	int bitsPerPixel;
	int padding;
	char* data;
};

char* loadFileToBuffer(const char* fileName, size_t* loadedFileSize, size_t extraBytes, size_t offset)
{
	FILE* fd = fopen(fileName, "rb");

	if (! fd)
	{
		printf("Could not open file '%s'", fileName);
		return nullptr;
	}

	fseek(fd, 0, SEEK_END);
	size_t fileSize = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	const size_t totalBufferSize = fileSize + extraBytes;
	char* buffer = new char[totalBufferSize];
	if(! fread(buffer + offset, fileSize, 1, fd))
	{
		printf("Failed to read from file '%s'", fileName);
		delete[] buffer;
		buffer = nullptr;
	}

	if (loadedFileSize)
		*loadedFileSize = fileSize;

	fclose(fd);
	return buffer;
}

void unloadFileBuffer(const char* fileBuffer)
{
	delete[] fileBuffer;
}

void unloadImage(Image* image)
{
	unloadFileBuffer((const char*)image);
}

Image* loadImageBitmap(const char* fileName)
{
	const size_t imageHeaderSize = sizeof(Image);
	char* buffer = loadFileToBuffer(fileName, nullptr, imageHeaderSize, imageHeaderSize);

	if (buffer == nullptr)
	{
		printf("Failed to load image '%s': Unable to find or read from file", fileName);
		return nullptr;
	}

	BitmapHeader* bitmap = (BitmapHeader*) (buffer + imageHeaderSize);
	Image* image = (Image*) buffer;

	if (bitmap->type != BITMAP_SIGNATURE)
	{
		printf("Failed to load image '%s': Invalid bitmap file", fileName);
		unloadFileBuffer(buffer);
		return nullptr;
	}

	if (bitmap->compression != BITMAP_COMPRESSION_BI_BITFIELDS && bitmap->compression != 0)
	{
		printf("Failed to load image '%s': Unsuported bitmap compression (%d)", fileName, bitmap->compression);
		unloadFileBuffer(buffer);
		return nullptr;
	}

	image->width = bitmap->width;
	image->height = bitmap->height;
	image->bitsPerPixel = bitmap->bitCount;
	image->data = bitmap->offBits + (char*) bitmap;
	image->padding = ( bitmap->width % 4 != 0 ) ? bitmap->width = 4 - (bitmap->width % 4) : 0;

	if (bitmap->bitCount == 24 || bitmap->bitCount == 32)
	{
		// get color masks 
		unsigned int* maskPtr = (unsigned int*) (sizeof(BitmapHeader) + (char*)bitmap);
		unsigned int rMask = *maskPtr++;
		unsigned int gMask = *maskPtr++;
		unsigned int bMask = *maskPtr++;
		unsigned int aMask = ~(rMask | gMask | bMask);

		unsigned int rMaskShift = (rMask == 0xFF000000) ? 24 : (rMask == 0xFF0000) ? 16 : (rMask == 0xFF00) ? 8 : 0;
		unsigned int gMaskShift = (gMask == 0xFF000000) ? 24 : (gMask == 0xFF0000) ? 16 : (gMask == 0xFF00) ? 8 : 0;
		unsigned int bMaskShift = (bMask == 0xFF000000) ? 24 : (bMask == 0xFF0000) ? 16 : (bMask == 0xFF00) ? 8 : 0;
		unsigned int aMaskShift = (aMask == 0xFF000000) ? 24 : (aMask == 0xFF0000) ? 16 : (aMask == 0xFF00) ? 8 : 0;

		const int numPixels = image->width * image->height;
		const int bytesPerPixel = image->bitsPerPixel / 8;

		for(int i = 0; i < numPixels; ++i)
		{
			unsigned int* pixelPtr =(unsigned int*) (i * bytesPerPixel + image->data);
			unsigned int pixel = *pixelPtr;
			unsigned int r = (pixel & rMask) >> rMaskShift;
			unsigned int g = (pixel & gMask) >> gMaskShift;
			unsigned int b = (pixel & bMask) >> bMaskShift;
			unsigned int a = (pixel & aMask) >> aMaskShift;
			unsigned int color = a << 24 | b << 16 | g << 8 | r;
			*pixelPtr = color;
		}
	}
	else if (bitmap->bitCount != 16)
	{
		printf("Failed to load image '%s': Unsuported bitmap bit count", fileName);
		unloadImage(image);
		return nullptr;
	}

	return (Image*) buffer;
}

char sample(const Palette* palette, float intensity)
{
	if (intensity > 1.0f)
		intensity = 1.0f;
	if (intensity < 0.0f)
		intensity = 0.0f;
	int index = (int) floor(palette->count * intensity);
	char c = palette->chars[index];
	return c;
}

void printAsciiArtFromImage(const Image* image)
{
	//const char* chars	= "@%#*+=-:.";
	const char* chars =	"$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'.";

	Palette palette;
	palette.chars = chars; 
	palette.count = (int) strlen(chars);

	for (int line = image->height - 1; line >= 0; line--)
	{
		char buffer[1080];
		char* p = buffer;

		for (int column = 0; column < image->width; column++)
		{
			float intensity = 0.0f;
			int r, g, b;

			if (image->bitsPerPixel == 32)
			{
				int pixel = ((int *)image->data)[image->width * line + column];
				r = pixel & 0x000000FF;
				g = (pixel & 0x0000FF00) >> 8;
				b = (pixel & 0x00FF0000) >> 16;
			}
			else if (image->bitsPerPixel == 24)
			{
				char* p = (&((char*)image->data)[image->width * 3 * line + column * 3 + (line * image->padding)]);
				r = *p;
				g = *(p+1);
				b = *(p+2);
			}
			else
			{
				printf("Only 32 and 24 bpp bitmaps are supported!\n");
				return;
			}

			intensity = (r + g + b) / 3.0f;
			*p++ = sample(&palette, intensity / 255.0f);
		}

		printf("%.*s\n", image->width, buffer);
	}

	return;
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("usage:\n adump file\n");
		return 0;
	}

	const char* filePath = argv[1];
	const Image* img = loadImageBitmap(filePath);
	if (img)
	{
		printAsciiArtFromImage(img);
		unloadImage((Image*)img);
		return 0;
	}
	return 1;
}

