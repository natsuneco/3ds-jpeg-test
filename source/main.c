#include <3ds.h>
// #include <citro3d.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>

unsigned int mynext_pow2(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v >= 64 ? v : 64;
}

bool loadJpeg(const char* filename, C3D_Tex* tex, C2D_Image* img) {
    // Declaring structs
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    // Load file
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        puts("Unable to open file!");
        return false;
    }
    jpeg_stdio_src(&cinfo, fp);

    // Read header
    jpeg_read_header(&cinfo, true);
    int width  = cinfo.image_width;
    int height = cinfo.image_height;
    int ch = cinfo.num_components;
    
    printf("JPEG header was loaded.\n");
    printf("w: %d, h: %d\n", width, height);
    printf("ch: %d\n", ch);

    jpeg_start_decompress(&cinfo);
    puts("Decompress started.");

    // Alloc memorys
    JSAMPARRAY data = (JSAMPARRAY)malloc(sizeof(JSAMPROW) * height);
    if (data == NULL) {
        printf("Failed to allocate memory for row pointers\n");
        return false;
    }

    // Alloc memorys
    for (int y = 0; y < height; y++) {
        data[y] = (JSAMPROW)malloc(sizeof(JSAMPLE) * width * ch);
        if (data[y] == NULL) {
            printf("Failed to allocate memory for row %d\n", y);
            // 確保済みのメモリを解放して終了
            for (int i = 0; i < y; i++) {
                free(data[i]);
            }
            free(data);
            return false;
        }
    }

    // Load data
    for (int y = 0; y < height; y++) {
        jpeg_read_scanlines(&cinfo, &data[y], 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(fp);

    // ----------------------
    img->tex = tex;

    unsigned int texWidth = mynext_pow2(width);
    unsigned int texHeight = mynext_pow2(height);

    if(texWidth > 1024 || texHeight > 1024)
    {
        printf("3DS only supports up to 1024x1024 textures. Reduce the image size.\n");
        return false;
    }

    Tex3DS_SubTexture *subt3x = (Tex3DS_SubTexture*)malloc(sizeof(Tex3DS_SubTexture));
    subt3x->width = 400;
    subt3x->height = 400;
    subt3x->left = 0.0f;
    subt3x->top = 1.0f;
    subt3x->right = width / (float)texWidth;
    subt3x->bottom = 1.0 - (height / (float)texHeight);
    img->subtex = subt3x;

    C3D_TexInit(img->tex, texWidth, texHeight, GPU_RGBA8);
    C3D_TexSetFilter(img->tex, GPU_LINEAR, GPU_NEAREST);

    memset(img->tex->data, 0, img->tex->size);

    // ------------------------
    for (int y = 0; y < height; y++) {
        JSAMPROW row = data[y];
        for (int x = 0; x < width; x++) {
            int idx = x * 3;
            JSAMPLE R = row[idx + 0];
            JSAMPLE G = row[idx + 1];
            JSAMPLE B = row[idx + 2];

            if ((x == 256 && y == 256) || (x == 1000 && y == 1000)) {
                printf("(%d, %d): (%d, %d, %d)\n", x, y, R, G, B);
            }

            // Convart to argb
            unsigned argb = (255) + (B << 8) + (G << 16) + (R << 24);

            // Get memory position to write
            u32 dst = (y * texWidth + x) * 4;

            // Write to texture
            memcpy((u8*)img->tex->data + dst, &argb, sizeof(u32));
        }
    }

    // Free
    for (int y = 0; y < height; y++) {
        free(data[y]);
    }
    free(data);

    return true;
}


int main(int argc, char* argv[]) {
    // Init libs
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    consoleInit(GFX_BOTTOM, NULL);

    // Create screen target
    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

    puts("Launched!");

    // Load texture
    C3D_Tex tex;
    C2D_Image img;
    if (!loadJpeg("test.jpg", &tex, &img)) {
        printf("Failed to load JPEG image\n");
    } else
        puts("JPEG loaded!");

    // Main loop
    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        // Bigin frame
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
        C2D_SceneBegin(top);

        // Draw image
        C2D_DrawImageAt(img, 0, 0, 0.0f, NULL, 0.5f, 0.5f);

        // End frame
        C3D_FrameEnd(0);
    }

    // Delete texture
    C3D_TexDelete(&tex);

    // Exit
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
