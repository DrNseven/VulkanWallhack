//main.h

// Highlight fragment shaders. Compiled with glslang (SPIR-V 1.0).
// If you want to change this shader, AI can compile it for you, ask claude or grok ect.

//Animated shader
static const uint32_t CustomFragmentShader[] = {
    0x07230203, 0x00010000, 0x00080008, 0x000000be, 0x00000000, 0x00020011,
    0x00000001, 0x00020011, 0x000013bf, 0x0007000a, 0x5f565053, 0x5f52484b,
    0x64616873, 0x635f7265, 0x6b636f6c, 0x00000000, 0x0006000b, 0x00000001,
    0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000,
    0x00000001, 0x0007000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000,
    0x00000020, 0x000000b6, 0x00030010, 0x00000004, 0x00000007, 0x00040047,
    0x00000015, 0x00000001, 0x00000000, 0x00040047, 0x00000020, 0x0000000b,
    0x0000000f, 0x00040047, 0x000000b6, 0x0000001e, 0x00000000, 0x00040047,
    0x000000b8, 0x00000001, 0x00000001, 0x00020013, 0x00000002, 0x00030021,
    0x00000003, 0x00000002, 0x00040015, 0x00000006, 0x00000020, 0x00000000,
    0x00040017, 0x00000007, 0x00000006, 0x00000002, 0x00040020, 0x00000008,
    0x00000007, 0x00000007, 0x0004002b, 0x00000006, 0x0000000a, 0x00000001,
    0x00030016, 0x0000000c, 0x00000020, 0x00040020, 0x0000000d, 0x00000007,
    0x0000000c, 0x0004002b, 0x00000006, 0x0000000f, 0x00000000, 0x00040020,
    0x00000010, 0x00000007, 0x00000006, 0x0004002b, 0x0000000c, 0x00000014,
    0x40c90fdb, 0x00040032, 0x00000006, 0x00000015, 0x00000001, 0x0004002b,
    0x0000000c, 0x00000018, 0x4f800000, 0x00040017, 0x0000001b, 0x0000000c,
    0x00000002, 0x00040020, 0x0000001c, 0x00000007, 0x0000001b, 0x00040017,
    0x0000001e, 0x0000000c, 0x00000004, 0x00040020, 0x0000001f, 0x00000001,
    0x0000001e, 0x0004003b, 0x0000001f, 0x00000020, 0x00000001, 0x0004002b,
    0x0000000c, 0x00000023, 0x3ca3d70a, 0x0004002b, 0x0000000c, 0x00000026,
    0x00000000, 0x0004002b, 0x0000000c, 0x00000029, 0x3fd9999a, 0x0004002b,
    0x0000000c, 0x00000032, 0x40133333, 0x0004002b, 0x0000000c, 0x00000034,
    0x40000000, 0x0004002b, 0x0000000c, 0x00000040, 0x3f8ccccd, 0x0004002b,
    0x0000000c, 0x00000048, 0x40e00000, 0x0004002b, 0x0000000c, 0x00000049,
    0x40a00000, 0x0005002c, 0x0000001b, 0x0000004a, 0x00000048, 0x00000049,
    0x0004002b, 0x0000000c, 0x0000004e, 0x40400000, 0x0004002b, 0x0000000c,
    0x00000057, 0x3f4ccccd, 0x0004002b, 0x0000000c, 0x0000005a, 0x3fa66666,
    0x0004002b, 0x0000000c, 0x0000006a, 0x40466666, 0x0004002b, 0x0000000c,
    0x0000006e, 0x402ccccd, 0x0004002b, 0x0000000c, 0x00000077, 0x3e4ccccd,
    0x0004002b, 0x0000000c, 0x0000007c, 0x3f000000, 0x0004002b, 0x0000000c,
    0x00000080, 0x3f800000, 0x0004002b, 0x0000000c, 0x00000081, 0x3e3851ec,
    0x0004002b, 0x0000000c, 0x00000087, 0x3f3851ec, 0x0004002b, 0x0000000c,
    0x00000088, 0x3f733333, 0x0004002b, 0x0000000c, 0x0000008b, 0x3f266666,
    0x0004002b, 0x0000000c, 0x0000008c, 0x3eb33333, 0x0004002b, 0x0000000c,
    0x00000090, 0x40c00000, 0x00040017, 0x00000098, 0x0000000c, 0x00000003,
    0x00040020, 0x00000099, 0x00000007, 0x00000098, 0x0004002b, 0x0000000c,
    0x0000009b, 0x3e99999a, 0x0006002c, 0x00000098, 0x0000009c, 0x0000009b,
    0x00000026, 0x0000009b, 0x0004002b, 0x0000000c, 0x0000009e, 0x3f59999a,
    0x0006002c, 0x00000098, 0x0000009f, 0x0000009e, 0x00000026, 0x0000009e,
    0x0006002c, 0x00000098, 0x000000a1, 0x00000080, 0x0000009b, 0x00000080,
    0x0004002b, 0x0000000c, 0x000000a3, 0x3e8f5c29, 0x0006002c, 0x00000098,
    0x000000a4, 0x00000080, 0x000000a3, 0x00000023, 0x00040020, 0x000000b5,
    0x00000003, 0x0000001e, 0x0004003b, 0x000000b5, 0x000000b6, 0x00000003,
    0x00040032, 0x0000000c, 0x000000b8, 0x41200000,   // ← was 0x40200000 (2.5)  →  now 10.0
    0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8,
    0x00000005, 0x0004003b, 0x00000008, 0x00000009, 0x00000007, 0x0004003b,
    0x0000000d, 0x0000000e, 0x00000007, 0x0004003b, 0x0000001c, 0x0000001d,
    0x00000007, 0x0004003b, 0x0000000d, 0x00000025, 0x00000007, 0x0004003b,
    0x0000001c, 0x00000055, 0x00000007, 0x0004003b, 0x0000000d, 0x0000007a,
    0x00000007, 0x0004003b, 0x0000000d, 0x0000007f, 0x00000007, 0x0004003b,
    0x0000000d, 0x00000086, 0x00000007, 0x0004003b, 0x00000099, 0x0000009a,
    0x00000007, 0x0004003b, 0x00000099, 0x0000009d, 0x00000007, 0x0004003b,
    0x00000099, 0x000000a0, 0x00000007, 0x0004003b, 0x00000099, 0x000000a2,
    0x00000007, 0x0004003b, 0x00000099, 0x000000a5, 0x00000007, 0x000413c0,
    0x00000007, 0x0000000b, 0x0000000a, 0x0003003e, 0x00000009, 0x0000000b,
    0x00050041, 0x00000010, 0x00000011, 0x00000009, 0x0000000f, 0x0004003d,
    0x00000006, 0x00000012, 0x00000011, 0x00040070, 0x0000000c, 0x00000013,
    0x00000012, 0x00040070, 0x0000000c, 0x00000016, 0x00000015, 0x00050085,
    0x0000000c, 0x00000017, 0x00000014, 0x00000016, 0x00050088, 0x0000000c,
    0x00000019, 0x00000017, 0x00000018, 0x00050085, 0x0000000c, 0x0000001a,
    0x00000013, 0x00000019, 0x0003003e, 0x0000000e, 0x0000001a, 0x0004003d,
    0x0000001e, 0x00000021, 0x00000020, 0x0007004f, 0x0000001b, 0x00000022,
    0x00000021, 0x00000021, 0x00000000, 0x00000001, 0x0005008e, 0x0000001b,
    0x00000024, 0x00000022, 0x00000023, 0x0003003e, 0x0000001d, 0x00000024,
    0x0003003e, 0x00000025, 0x00000026, 0x00050041, 0x0000000d, 0x00000027,
    0x0000001d, 0x0000000f, 0x0004003d, 0x0000000c, 0x00000028, 0x00000027,
    0x00050085, 0x0000000c, 0x0000002a, 0x00000028, 0x00000029, 0x0004003d,
    0x0000000c, 0x0000002b, 0x0000000e, 0x00050081, 0x0000000c, 0x0000002c,
    0x0000002a, 0x0000002b, 0x0006000c, 0x0000000c, 0x0000002d, 0x00000001,
    0x0000000d, 0x0000002c, 0x0004003d, 0x0000000c, 0x0000002e, 0x00000025,
    0x00050081, 0x0000000c, 0x0000002f, 0x0000002e, 0x0000002d, 0x0003003e,
    0x00000025, 0x0000002f, 0x00050041, 0x0000000d, 0x00000030, 0x0000001d,
    0x0000000a, 0x0004003d, 0x0000000c, 0x00000031, 0x00000030, 0x00050085,
    0x0000000c, 0x00000033, 0x00000031, 0x00000032, 0x0004003d, 0x0000000c,
    0x00000035, 0x0000000e, 0x00050085, 0x0000000c, 0x00000036, 0x00000034,
    0x00000035, 0x00050083, 0x0000000c, 0x00000037, 0x00000033, 0x00000036,
    0x0006000c, 0x0000000c, 0x00000038, 0x00000001, 0x0000000d, 0x00000037,
    0x0004003d, 0x0000000c, 0x00000039, 0x00000025, 0x00050081, 0x0000000c,
    0x0000003a, 0x00000039, 0x00000038, 0x0003003e, 0x00000025, 0x0000003a,
    0x00050041, 0x0000000d, 0x0000003b, 0x0000001d, 0x0000000f, 0x0004003d,
    0x0000000c, 0x0000003c, 0x0000003b, 0x00050041, 0x0000000d, 0x0000003d,
    0x0000001d, 0x0000000a, 0x0004003d, 0x0000000c, 0x0000003e, 0x0000003d,
    0x00050081, 0x0000000c, 0x0000003f, 0x0000003c, 0x0000003e, 0x00050085,
    0x0000000c, 0x00000041, 0x0000003f, 0x00000040, 0x0004003d, 0x0000000c,
    0x00000042, 0x0000000e, 0x00050081, 0x0000000c, 0x00000043, 0x00000041,
    0x00000042, 0x0006000c, 0x0000000c, 0x00000044, 0x00000001, 0x0000000d,
    0x00000043, 0x0004003d, 0x0000000c, 0x00000045, 0x00000025, 0x00050081,
    0x0000000c, 0x00000046, 0x00000045, 0x00000044, 0x0003003e, 0x00000025,
    0x00000046, 0x0004003d, 0x0000001b, 0x00000047, 0x0000001d, 0x00050083,
    0x0000001b, 0x0000004b, 0x00000047, 0x0000004a, 0x0006000c, 0x0000000c,
    0x0000004c, 0x00000001, 0x00000042, 0x0000004b, 0x00050085, 0x0000000c,
    0x0000004d, 0x0000004c, 0x00000034, 0x0004003d, 0x0000000c, 0x0000004f,
    0x0000000e, 0x00050085, 0x0000000c, 0x00000050, 0x0000004e, 0x0000004f,
    0x00050083, 0x0000000c, 0x00000051, 0x0000004d, 0x00000050, 0x0006000c,
    0x0000000c, 0x00000052, 0x00000001, 0x0000000d, 0x00000051, 0x0004003d,
    0x0000000c, 0x00000053, 0x00000025, 0x00050081, 0x0000000c, 0x00000054,
    0x00000053, 0x00000052, 0x0003003e, 0x00000025, 0x00000054, 0x0004003d,
    0x0000001b, 0x00000056, 0x0000001d, 0x00050041, 0x0000000d, 0x00000058,
    0x0000001d, 0x0000000a, 0x0004003d, 0x0000000c, 0x00000059, 0x00000058,
    0x00050085, 0x0000000c, 0x0000005b, 0x00000059, 0x0000005a, 0x0004003d,
    0x0000000c, 0x0000005c, 0x0000000e, 0x00050081, 0x0000000c, 0x0000005d,
    0x0000005b, 0x0000005c, 0x0006000c, 0x0000000c, 0x0000005e, 0x00000001,
    0x0000000d, 0x0000005d, 0x00050041, 0x0000000d, 0x0000005f, 0x0000001d,
    0x0000000f, 0x0004003d, 0x0000000c, 0x00000060, 0x0000005f, 0x00050085,
    0x0000000c, 0x00000061, 0x00000060, 0x00000040, 0x0004003d, 0x0000000c,
    0x00000062, 0x0000000e, 0x00050083, 0x0000000c, 0x00000063, 0x00000061,
    0x00000062, 0x0006000c, 0x0000000c, 0x00000064, 0x00000001, 0x0000000e,
    0x00000063, 0x00050050, 0x0000001b, 0x00000065, 0x0000005e, 0x00000064,
    0x0005008e, 0x0000001b, 0x00000066, 0x00000065, 0x00000057, 0x00050081,
    0x0000001b, 0x00000067, 0x00000056, 0x00000066, 0x0003003e, 0x00000055,
    0x00000067, 0x00050041, 0x0000000d, 0x00000068, 0x00000055, 0x0000000f,
    0x0004003d, 0x0000000c, 0x00000069, 0x00000068, 0x00050085, 0x0000000c,
    0x0000006b, 0x00000069, 0x0000006a, 0x00050041, 0x0000000d, 0x0000006c,
    0x00000055, 0x0000000a, 0x0004003d, 0x0000000c, 0x0000006d, 0x0000006c,
    0x00050085, 0x0000000c, 0x0000006f, 0x0000006d, 0x0000006e, 0x00050081,
    0x0000000c, 0x00000070, 0x0000006b, 0x0000006f, 0x0004003d, 0x0000000c,
    0x00000071, 0x0000000e, 0x00050085, 0x0000000c, 0x00000072, 0x00000034,
    0x00000071, 0x00050081, 0x0000000c, 0x00000073, 0x00000070, 0x00000072,
    0x0006000c, 0x0000000c, 0x00000074, 0x00000001, 0x0000000d, 0x00000073,
    0x0004003d, 0x0000000c, 0x00000075, 0x00000025, 0x00050081, 0x0000000c,
    0x00000076, 0x00000075, 0x00000074, 0x0003003e, 0x00000025, 0x00000076,
    0x0004003d, 0x0000000c, 0x00000078, 0x00000025, 0x00050085, 0x0000000c,
    0x00000079, 0x00000078, 0x00000077, 0x0003003e, 0x00000025, 0x00000079,
    0x0004003d, 0x0000000c, 0x0000007b, 0x00000025, 0x00050085, 0x0000000c,
    0x0000007d, 0x0000007b, 0x0000007c, 0x00050081, 0x0000000c, 0x0000007e,
    0x0000007d, 0x0000007c, 0x0003003e, 0x0000007a, 0x0000007e, 0x0004003d,
    0x0000000c, 0x00000082, 0x00000025, 0x0006000c, 0x0000000c, 0x00000083,
    0x00000001, 0x00000004, 0x00000082, 0x0008000c, 0x0000000c, 0x00000084,
    0x00000001, 0x00000031, 0x00000026, 0x00000081, 0x00000083, 0x00050083,
    0x0000000c, 0x00000085, 0x00000080, 0x00000084, 0x0003003e, 0x0000007f,
    0x00000085, 0x0004003d, 0x0000000c, 0x00000089, 0x0000007a, 0x0008000c,
    0x0000000c, 0x0000008a, 0x00000001, 0x00000031, 0x00000087, 0x00000088,
    0x00000089, 0x0003003e, 0x00000086, 0x0000008a, 0x0004003d, 0x0000000c,
    0x0000008d, 0x0000000e, 0x00050085, 0x0000000c, 0x0000008e, 0x00000034,
    0x0000008d, 0x0004003d, 0x0000000c, 0x0000008f, 0x0000007a, 0x00050085,
    0x0000000c, 0x00000091, 0x0000008f, 0x00000090, 0x00050081, 0x0000000c,
    0x00000092, 0x0000008e, 0x00000091, 0x0006000c, 0x0000000c, 0x00000093,
    0x00000001, 0x0000000d, 0x00000092, 0x00050085, 0x0000000c, 0x00000094,
    0x0000008c, 0x00000093, 0x00050081, 0x0000000c, 0x00000095, 0x0000008b,
    0x00000094, 0x0004003d, 0x0000000c, 0x00000096, 0x00000086, 0x00050085,
    0x0000000c, 0x00000097, 0x00000096, 0x00000095, 0x0003003e, 0x00000086,
    0x00000097, 0x0003003e, 0x0000009a, 0x0000009c, 0x0003003e, 0x0000009d,
    0x0000009f, 0x0003003e, 0x000000a0, 0x000000a1, 0x0003003e, 0x000000a2,
    0x000000a4, 0x0004003d, 0x00000098, 0x000000a6, 0x0000009a, 0x0004003d,
    0x00000098, 0x000000a7, 0x0000009d, 0x0004003d, 0x0000000c, 0x000000a8,
    0x0000007a, 0x00060050, 0x00000098, 0x000000a9, 0x000000a8, 0x000000a8,
    0x000000a8, 0x0008000c, 0x00000098, 0x000000aa, 0x00000001, 0x0000002e,
    0x000000a6, 0x000000a7, 0x000000a9, 0x0003003e, 0x000000a5, 0x000000aa,
    0x0004003d, 0x00000098, 0x000000ab, 0x000000a5, 0x0004003d, 0x00000098,
    0x000000ac, 0x000000a0, 0x0004003d, 0x0000000c, 0x000000ad, 0x0000007f,
    0x00060050, 0x00000098, 0x000000ae, 0x000000ad, 0x000000ad, 0x000000ad,
    0x0008000c, 0x00000098, 0x000000af, 0x00000001, 0x0000002e, 0x000000ab,
    0x000000ac, 0x000000ae, 0x0003003e, 0x000000a5, 0x000000af, 0x0004003d,
    0x00000098, 0x000000b0, 0x000000a5, 0x0004003d, 0x00000098, 0x000000b1,
    0x000000a2, 0x0004003d, 0x0000000c, 0x000000b2, 0x00000086, 0x00060050,
    0x00000098, 0x000000b3, 0x000000b2, 0x000000b2, 0x000000b2, 0x0008000c,
    0x00000098, 0x000000b4, 0x00000001, 0x0000002e, 0x000000b0, 0x000000b1,
    0x000000b3, 0x0003003e, 0x000000a5, 0x000000b4, 0x0004003d, 0x00000098,
    0x000000b7, 0x000000a5, 0x0005008e, 0x00000098, 0x000000b9, 0x000000b7,
    0x000000b8, 0x00050051, 0x0000000c, 0x000000ba, 0x000000b9, 0x00000000,
    0x00050051, 0x0000000c, 0x000000bb, 0x000000b9, 0x00000001, 0x00050051,
    0x0000000c, 0x000000bc, 0x000000b9, 0x00000002, 0x00070050, 0x0000001e,
    0x000000bd, 0x000000ba, 0x000000bb, 0x000000bc, 0x00000080, 0x0003003e,
    0x000000b6, 0x000000bd, 0x000100fd, 0x00010038,
};

/*
// non animated simple flat color shaders 
// flat-red fragment shader
static const uint32_t CustomFragmentShader[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000c, 0x00000000, 0x00020011,
    0x00000001, 0x0003000e, 0x00000000, 0x00000001, 0x0006000f, 0x00000004,
    0x00000001, 0x6e69616d, 0x00000000, 0x00000002, 0x00030010, 0x00000001,
    0x00000007, 0x00040047, 0x00000002, 0x0000001e, 0x00000000, 0x00020013,
    0x00000003, 0x00030021, 0x00000004, 0x00000003, 0x00030016, 0x00000005,
    0x00000020, 0x00040017, 0x00000006, 0x00000005, 0x00000004, 0x00040020,
    0x00000007, 0x00000003, 0x00000006, 0x0004003b, 0x00000007, 0x00000002,
    0x00000003, 0x0004002b, 0x00000005, 0x00000008, 0x3f800000, 0x0004002b,
    0x00000005, 0x00000009, 0x00000000, 0x0007002c, 0x00000006, 0x0000000a,
    0x00000008, 0x00000009, 0x00000009, 0x00000008, 0x00050036, 0x00000003,
    0x00000001, 0x00000000, 0x00000004, 0x000200f8, 0x0000000b, 0x0003003e,
    0x00000002, 0x0000000a, 0x000100fd, 0x00010038,
};

// magenta: o = vec4(1.0, 0.0, 1.0, 1.0)
static const uint32_t CustomFragmentShader[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000c, 0x00000000, 0x00020011,
    0x00000001, 0x0003000e, 0x00000000, 0x00000001, 0x0006000f, 0x00000004,
    0x00000001, 0x6e69616d, 0x00000000, 0x00000002, 0x00030010, 0x00000001,
    0x00000007, 0x00040047, 0x00000002, 0x0000001e, 0x00000000, 0x00020013,
    0x00000003, 0x00030021, 0x00000004, 0x00000003, 0x00030016, 0x00000005,
    0x00000020, 0x00040017, 0x00000006, 0x00000005, 0x00000004, 0x00040020,
    0x00000007, 0x00000003, 0x00000006, 0x0004003b, 0x00000007, 0x00000002,
    0x00000003, 0x0004002b, 0x00000005, 0x00000008, 0x3f800000, 0x0004002b,
    0x00000005, 0x00000009, 0x00000000, 0x0007002c, 0x00000006, 0x0000000a,
    0x00000008, 0x00000009, 0x00000008, 0x00000008, 0x00050036, 0x00000003,
    0x00000001, 0x00000000, 0x00000004, 0x000200f8, 0x0000000b, 0x0003003e,
    0x00000002, 0x0000000a, 0x000100fd, 0x00010038,
};

// green: o = vec4(0.0, 1.0, 0.0, 1.0)
static const uint32_t CustomFragmentShader[] = {
    0x07230203, 0x00010000, 0x00000000, 0x0000000c, 0x00000000, 0x00020011,
    0x00000001, 0x0003000e, 0x00000000, 0x00000001, 0x0006000f, 0x00000004,
    0x00000001, 0x6e69616d, 0x00000000, 0x00000002, 0x00030010, 0x00000001,
    0x00000007, 0x00040047, 0x00000002, 0x0000001e, 0x00000000, 0x00020013,
    0x00000003, 0x00030021, 0x00000004, 0x00000003, 0x00030016, 0x00000005,
    0x00000020, 0x00040017, 0x00000006, 0x00000005, 0x00000004, 0x00040020,
    0x00000007, 0x00000003, 0x00000006, 0x0004003b, 0x00000007, 0x00000002,
    0x00000003, 0x0004002b, 0x00000005, 0x00000008, 0x3f800000, 0x0004002b,
    0x00000005, 0x00000009, 0x00000000, 0x0007002c, 0x00000006, 0x0000000a,
    0x00000009, 0x00000008, 0x00000009, 0x00000008, 0x00050036, 0x00000003,
    0x00000001, 0x00000000, 0x00000004, 0x000200f8, 0x0000000b, 0x0003003e,
    0x00000002, 0x0000000a, 0x000100fd, 0x00010038,
};
*/

/*
// implement this shit if shader animations don't work in a game, so far it works without it
// ============================================================================
// DeviceClockSupport.cpp
//
// Gives the highlight fragment shader a time source WITHOUT touching pipeline
// layouts: the animated plasma shader reads the GPU's device-wide realtime clock
// (VK_KHR_shader_clock, feature shaderDeviceClock). The game doesn't enable that,
// so vkCreateDevice is hooked to add the extension + feature when the GPU has them.
//
// Needs (adapt the names to what you already have):
//   pOriginalCreateDevice, pGetPhysicalDeviceProperties,
//   pGetPhysicalDeviceFeatures2, pEnumerateDeviceExtensionProperties
//   (instance-level functions; resolve them with vkGetInstanceProcAddr)
//   kPlasmaFragSpv  (HighlightShaderPlasma.h)
//   kPlasmaAnimFragSpv (HighlightShaderPlasmaAnimated.h)
//
// The hook must be active BEFORE the game calls vkCreateDevice. If the device was
// already created without the feature, g_useClockShader stays false and the static
// plasma is used instead (nothing breaks).
// ============================================================================

#include <vector>
#include <cstring>
#include <cstddef>

bool g_useClockShader = false;       // true only if the device really has shaderDeviceClock enabled

// Specialization constant 0 = N_CYCLES (animation cycles per 2^32 clock ticks).
//   speed [cycles/s] = N * clockHz / 2^32      ->  N = round(speed * 2^32 / clockHz)
// For ~0.25 cycles/s:  1 GHz (1 tick = 1 ns) -> N = 1,  100 MHz -> N = 11.
//
// Specialization constant 1 = GAIN (float): overall brightness of the overlay. Because the
// overlay is written into the scene color BEFORE exposure / tonemapping / fog / darkness
// passes, it is dimmed together with the scene; GAIN counters that. Start at 2.5; if it is
// still too weak in dark areas try 4-8. Don't go extreme: very large values (the old 55x)
// can crush auto-exposure or overflow fp16 and turn the screen black.
struct HighlightSpecData
{
    uint32_t cycles = 1;      // constant_id 0
    float    gain = 2.5f;     // constant_id 1
};
static HighlightSpecData g_specData;
static const VkSpecializationMapEntry g_specEntries[2] = {
    { 0, (uint32_t)offsetof(HighlightSpecData, cycles), sizeof(uint32_t) },
    { 1, (uint32_t)offsetof(HighlightSpecData, gain),   sizeof(float)    },
};
// Passed for EVERY highlight fragment stage: constants a shader doesn't declare are ignored.
static VkSpecializationInfo g_highlightSpecInfo = { 2, g_specEntries, sizeof(HighlightSpecData), &g_specData };

VKAPI_ATTR VkResult VKAPI_CALL DetourVkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice)
{
    VkDeviceCreateInfo ci = *pCreateInfo;
    std::vector<const char*> exts(pCreateInfo->ppEnabledExtensionNames,
                                  pCreateInfo->ppEnabledExtensionNames + pCreateInfo->enabledExtensionCount);
    VkPhysicalDeviceShaderClockFeaturesKHR clockOn{};
    bool added = false;          // true only if WE modified the create info

    // Did the game already enable the extension + shaderDeviceClock itself?
    bool gameHasClockExt = false, gameHasClockFeat = false;
    for (const char* e : exts)
        if (!strcmp(e, VK_KHR_SHADER_CLOCK_EXTENSION_NAME))
            gameHasClockExt = true;
    for (auto p = static_cast<const VkBaseInStructure*>(pCreateInfo->pNext); p; p = p->pNext)
        if (p->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR)
            gameHasClockFeat = reinterpret_cast<const VkPhysicalDeviceShaderClockFeaturesKHR*>(p)->shaderDeviceClock != VK_FALSE;
    const bool gameHasClock = gameHasClockExt && gameHasClockFeat;

    // Clock frequency differs per vendor, so only enable it where the speed is known.
    // NVIDIA: realtime clock is a nanosecond timer (assumption, tune N if the speed is off)
    // AMD:    100 MHz
    // others: not enabled -> static plasma
    VkPhysicalDeviceProperties props{};
    pGetPhysicalDeviceProperties(physicalDevice, &props);

    uint32_t cycles = 0;
    if (props.vendorID == 0x10DE)      cycles = 1;
    else if (props.vendorID == 0x1002) cycles = 11;

    if (!gameHasClock && cycles != 0)
    {
        uint32_t n = 0;
        pEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &n, nullptr);
        std::vector<VkExtensionProperties> avail(n);
        pEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &n, avail.data());

        bool hasExt = false;
        for (const auto& e : avail)
            if (!strcmp(e.extensionName, VK_KHR_SHADER_CLOCK_EXTENSION_NAME))
                hasExt = true;

        VkPhysicalDeviceShaderClockFeaturesKHR supported{};
        supported.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
        VkPhysicalDeviceFeatures2 f2{};
        f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        f2.pNext = &supported;
        if (hasExt)
            pGetPhysicalDeviceFeatures2(physicalDevice, &f2);

        if (hasExt && supported.shaderDeviceClock)
        {
            bool dup = false;
            for (const char* e : exts)
                if (!strcmp(e, VK_KHR_SHADER_CLOCK_EXTENSION_NAME))
                    dup = true;
            if (!dup)
                exts.push_back(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);

            clockOn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
            clockOn.shaderDeviceClock = VK_TRUE;
            clockOn.shaderSubgroupClock = VK_FALSE;
            clockOn.pNext = const_cast<void*>(pCreateInfo->pNext);   // our struct goes in front of the game's chain

            ci.pNext = &clockOn;
            ci.enabledExtensionCount = static_cast<uint32_t>(exts.size());
            ci.ppEnabledExtensionNames = exts.data();
            added = true;
        }
    }

    VkResult r = pOriginalCreateDevice(physicalDevice, &ci, pAllocator, pDevice);
    if (r != VK_SUCCESS && added)
    {
        // Never let our change break the game: retry exactly as the game asked.
        added = false;
        r = pOriginalCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    }

    if (r == VK_SUCCESS)
    {
        g_useClockShader = added || gameHasClock;   // note: last created device wins (fine for single-device games)
        g_specData.cycles = cycles ? cycles : 1;
        Log("DetourVkCreateDevice: clock shader %s (vendor=0x%x N=%u gain=%.2f)",
            gameHasClock ? "ENABLED BY THE GAME" : (added ? "ENABLED BY US" : "not available -> static plasma"),
            (unsigned)props.vendorID, g_specData.cycles, g_specData.gain);
    }
    return r;
}

// ---- replace your GetRedFragModule with this (same idea, picks the shader) ----

static VkShaderModule GetHighlightFragModule(VkDevice device)
{
    static std::mutex s_mtx;
    static std::unordered_map<VkDevice, VkShaderModule> s_mods;

    std::lock_guard<std::mutex> lk(s_mtx);
    auto it = s_mods.find(device);
    if (it != s_mods.end())
        return it->second;

    VkShaderModuleCreateInfo mci{};
    mci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    if (g_useClockShader) { mci.codeSize = sizeof(kPlasmaAnimFragSpv); mci.pCode = kPlasmaAnimFragSpv; }
    else                  { mci.codeSize = sizeof(kPlasmaFragSpv);     mci.pCode = kPlasmaFragSpv; }

    VkShaderModule m = VK_NULL_HANDLE;
    if (pOriginalCreateShaderModule(device, &mci, nullptr, &m) != VK_SUCCESS)
    {
        Log("GetHighlightFragModule: vkCreateShaderModule FAILED");
        m = VK_NULL_HANDLE;
    }
    s_mods[device] = m;
    return m;
}

// ---- in BuildHighlightCI (DetourVkCreateGraphicsPipelines_flatred.cpp) change ONE line ----
//
//     st.pSpecializationInfo = nullptr;
// to
//     st.pSpecializationInfo = &g_highlightSpecInfo;     // always: GAIN is used by both shaders
//
// and call GetHighlightFragModule(device) instead of GetRedFragModule(device).
// (g_useClockShader / g_highlightSpecInfo must be visible there; move the globals to a shared header if needed.)
//
// For the magenta shaders, in GetHighlightFragModule use kMagentaPlasmaAnimFragSpv /
// kMagentaPlasmaFragSpv (HighlightShaderMagentaPlasma.h) instead of the red plasma arrays.
*/
