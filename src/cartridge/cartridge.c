//
// Created by Seth on 9/11/2025.
//

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "cartridge.h"
#include "ines.h"

static uint8_t *cart_data = NULL;
static size_t cart_size = 0;

static void free_cart(void)
{
    free(cart_data);
    cart_data = NULL;
    cart_size = 0;
}

int cartridge_load(const char *path)
{
    free_cart();

    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "cartridge: failed to open %s\n", path);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (n <= 0)
    {
        fclose(f);
        fprintf(stderr, "cartridge: empty file\n");
        return -1;
    }

    cart_data = (uint8_t*)malloc((size_t)n);
    if (!cart_data)
    {
        fclose(f);
        fprintf(stderr, "cartridge: out of memory\n");
        return -1;
    }

    if (fread(cart_data, 1, (size_t)n, f) != (size_t)n)
    {
        fclose(f);
        free_cart();
        fprintf(stderr, "cartridge: short read\n");
        return -1;
    }
    fclose(f);
    cart_size = (size_t)n;

    int rc = ines_load(cart_data, cart_size);  // 0 = OK, nonzero = error
    if (rc != 0) {
        fprintf(stderr, "cartridge: ines_load failed (rc=%d)\n", rc);
        free_cart();                // your helper that frees s_cart_data, etc.
        return -1;                  // propagate failure
    }
    return 0;
}

void cartridge_unload(void)
{
    free_cart();
}