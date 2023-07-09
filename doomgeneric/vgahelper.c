/*
    Code here is mostly taken from ReactOS.

    256-color mode set function was taken from http://bos.asmhackers.net/docs/vga_without_bios/docs/mode%2013h%20without%20using%20bios.htm

    VGA default palette was generated with this program: https://github.com/canidlogic/vgapal/ as a 8-bits per channel palette.

    TODO: Cleanup.
*/

#include <ntifs.h>
#include <ntddk.h>
#include <wdm.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <ntddkbd.h>
#include <ntddvdeo.h>

#ifdef _M_X64
#define HAL_HalFindBusAddressTranslation_OFFSET 0x50
#else
#define HAL_HalFindBusAddressTranslation_OFFSET 0x28
#endif

#pragma pack(push, 1)

typedef struct _HAL_PRIVATE_DISPATCH_TABLE
{
	union
	{
		ULONG Version;
        struct {
            char pad[HAL_HalFindBusAddressTranslation_OFFSET];
            BOOLEAN (NTAPI *HalFindBusAddressTranslation)(PHYSICAL_ADDRESS, ULONG*, PHYSICAL_ADDRESS*, ULONG_PTR*, BOOLEAN);
            BOOLEAN (NTAPI *HalResetDisplay)(VOID);
        };
    };
} HAL_PRIVATE_DISPATCH_TABLE;

#pragma pack(pop)

extern NTSYSAPI HAL_PRIVATE_DISPATCH_TABLE HalPrivateDispatchTable;

// some useful equates - major commands

#define EOD     0x000                   // end of data
#define INOUT   0x010                   // do ins or outs
#define METAOUT 0x020                   // do special types of outs
#define NCMD    0x0f0                   // Nop command


// flags for INOUT major command

//#define UNUSED    0x01                    // reserved
#define MULTI   0x02                    // multiple or single outs
#define BW      0x04                    // byte/word size of operation
#define IO      0x08                    // out/in instruction

// minor commands for METAOUT

#define INDXOUT 0x00                    // do indexed outs
#define ATCOUT  0x01                    // do indexed outs for atc
#define MASKOUT 0x02                    // do masked outs using and-xor masks


// composite INOUT type commands

#define OB      (INOUT)                 // output 8 bit value
#define OBM     (INOUT+MULTI)           // output multiple bytes
#define OW      (INOUT+BW)              // output single word value
#define OWM     (INOUT+BW+MULTI)        // output multiple words

#define IB      (INOUT+IO)              // input byte
#define IBM     (INOUT+IO+MULTI)        // input multiple bytes
#define IW      (INOUT+IO+BW)           // input word
#define IWM     (INOUT+IO+BW+MULTI)     // input multiple words

//
// Base address of VGA memory range.  Also used as base address of VGA
// memory when loading a font, which is done with the VGA mapped at A0000.
//

#define MEM_VGA      0xA0000
#define MEM_VGA_SIZE 0x20000

//
// For memory mapped IO
//

#define MEMORY_MAPPED_IO_OFFSET (0xB8000 - 0xA0000)

//
// Port definitions for filling the ACCESS_RANGES structure in the miniport
// information, defines the range of I/O ports the VGA spans.
// There is a break in the IO ports - a few ports are used for the parallel
// port. Those cannot be defined in the ACCESS_RANGE, but are still mapped
// so all VGA ports are in one address range.
//

#define VGA_BASE_IO_PORT      0x000003B0
#define VGA_START_BREAK_PORT  0x000003BB
#define VGA_END_BREAK_PORT    0x000003C0
#define VGA_MAX_IO_PORT       0x000003DF

//
// VGA register definitions
//
// eVb: 3.1 [VGA] - Use offsets from the VGA Port Address instead of absolute
#define CRTC_ADDRESS_PORT_MONO      0x0004  // CRT Controller Address and
#define CRTC_DATA_PORT_MONO         0x0005  // Data registers in mono mode
#define FEAT_CTRL_WRITE_PORT_MONO   0x000A  // Feature Control write port
                                            // in mono mode
#define INPUT_STATUS_1_MONO         0x000A  // Input Status 1 register read
                                            // port in mono mode
#define ATT_INITIALIZE_PORT_MONO    INPUT_STATUS_1_MONO
                                            // Register to read to reset
                                            // Attribute Controller index/data

#define ATT_ADDRESS_PORT            0x0010  // Attribute Controller Address and
#define ATT_DATA_WRITE_PORT         0x0010  // Data registers share one port
                                            // for writes, but only Address is
                                            // readable at 0x3C0
#define ATT_DATA_READ_PORT          0x0011  // Attribute Controller Data reg is
                                            // readable here
#define MISC_OUTPUT_REG_WRITE_PORT  0x0012  // Miscellaneous Output reg write
                                            // port
#define INPUT_STATUS_0_PORT         0x0012  // Input Status 0 register read
                                            // port
#define VIDEO_SUBSYSTEM_ENABLE_PORT 0x0013  // Bit 0 enables/disables the
                                            // entire VGA subsystem
#define SEQ_ADDRESS_PORT            0x0014  // Sequence Controller Address and
#define SEQ_DATA_PORT               0x0015  // Data registers
#define DAC_PIXEL_MASK_PORT         0x0016  // DAC pixel mask reg
#define DAC_ADDRESS_READ_PORT       0x0017  // DAC register read index reg,
                                            // write-only
#define DAC_STATE_PORT              0x0017  // DAC state (read/write),
                                            // read-only
#define DAC_ADDRESS_WRITE_PORT      0x0018  // DAC register write index reg
#define DAC_DATA_REG_PORT           0x0019  // DAC data transfer reg
#define FEAT_CTRL_READ_PORT         0x001A  // Feature Control read port
#define MISC_OUTPUT_REG_READ_PORT   0x001C  // Miscellaneous Output reg read
                                            // port
#define GRAPH_ADDRESS_PORT          0x001E  // Graphics Controller Address
#define GRAPH_DATA_PORT             0x001F  // and Data registers

#define CRTC_ADDRESS_PORT_COLOR     0x0024  // CRT Controller Address and
#define CRTC_DATA_PORT_COLOR        0x0025  // Data registers in color mode
#define FEAT_CTRL_WRITE_PORT_COLOR  0x002A  // Feature Control write port
#define INPUT_STATUS_1_COLOR        0x002A  // Input Status 1 register read
                                            // port in color mode
// eVb: 3.2 [END]
#define ATT_INITIALIZE_PORT_COLOR   INPUT_STATUS_1_COLOR
                                            // Register to read to reset
                                            // Attribute Controller index/data
                                            // toggle in color mode

//
// VGA indexed register indexes.
//

#define IND_CURSOR_START        0x0A    // index in CRTC of the Cursor Start
#define IND_CURSOR_END          0x0B    //  and End registers
#define IND_CURSOR_HIGH_LOC     0x0E    // index in CRTC of the Cursor Location
#define IND_CURSOR_LOW_LOC      0x0F    //  High and Low Registers
#define IND_VSYNC_END           0x11    // index in CRTC of the Vertical Sync
                                        //  End register, which has the bit
                                        //  that protects/unprotects CRTC
                                        //  index registers 0-7
#define IND_CR2C                0x2C    // Nordic LCD Interface Register
#define IND_CR2D                0x2D    // Nordic LCD Display Control
#define IND_SET_RESET_ENABLE    0x01    // index of Set/Reset Enable reg in GC
#define IND_DATA_ROTATE         0x03    // index of Data Rotate reg in GC
#define IND_READ_MAP            0x04    // index of Read Map reg in Graph Ctlr
#define IND_GRAPH_MODE          0x05    // index of Mode reg in Graph Ctlr
#define IND_GRAPH_MISC          0x06    // index of Misc reg in Graph Ctlr
#define IND_BIT_MASK            0x08    // index of Bit Mask reg in Graph Ctlr
#define IND_SYNC_RESET          0x00    // index of Sync Reset reg in Seq
#define IND_MAP_MASK            0x02    // index of Map Mask in Sequencer
#define IND_MEMORY_MODE         0x04    // index of Memory Mode reg in Seq
#define IND_CRTC_PROTECT        0x11    // index of reg containing regs 0-7 in
                                        //  CRTC
#define IND_CRTC_COMPAT         0x34    // index of CRTC Compatibility reg
                                        //  in CRTC
#define IND_PERF_TUNING         0x16    // index of performance tuning in Seq
#define START_SYNC_RESET_VALUE  0x01    // value for Sync Reset reg to start
                                        //  synchronous reset
#define END_SYNC_RESET_VALUE    0x03    // value for Sync Reset reg to end
                                        //  synchronous reset

//
// Values for Attribute Controller Index register to turn video off
// and on, by setting bit 5 to 0 (off) or 1 (on).
//

#define VIDEO_DISABLE 0
#define VIDEO_ENABLE  0x20

#define INDEX_ENABLE_AUTO_START 0x31

// Masks to keep only the significant bits of the Graphics Controller and
// Sequencer Address registers. Masking is necessary because some VGAs, such
// as S3-based ones, don't return unused bits set to 0, and some SVGAs use
// these bits if extensions are enabled.
//

#define GRAPH_ADDR_MASK 0x0F
#define SEQ_ADDR_MASK   0x07

//
// Mask used to toggle Chain4 bit in the Sequencer's Memory Mode register.
//

#define CHAIN4_MASK 0x08

//
// Value written to the Read Map register when identifying the existence of
// a VGA in VgaInitialize. This value must be different from the final test
// value written to the Bit Mask in that routine.
//

#define READ_MAP_TEST_SETTING 0x03

//
// Default text mode setting for various registers, used to restore their
// states if VGA detection fails after they've been modified.
//

#define MEMORY_MODE_TEXT_DEFAULT 0x02
#define BIT_MASK_DEFAULT 0xFF
#define READ_MAP_DEFAULT 0x00


//
// Palette-related info.
//

//
// Highest valid DAC color register index.
//

#define VIDEO_MAX_COLOR_REGISTER  0xFF

//
// Highest valid palette register index
//

#define VIDEO_MAX_PALETTE_REGISTER 0x0F

//
// Mode into which to put the VGA before starting a VDM, so it's a plain
// vanilla VGA.  (This is the mode's index in ModesVGA[], currently standard
// 80x25 text mode.)
//

#define DEFAULT_MODE 0

//
// Number of bytes to save in each plane.
//

#define VGA_PLANE_SIZE 0x10000

//
// Number of each type of indexed register in a standard VGA, used by
// validator and state save/restore functions.
//

#define VGA_NUM_SEQUENCER_PORTS     5
#define VGA_NUM_CRTC_PORTS         25
#define VGA_NUM_GRAPH_CONT_PORTS    9
#define VGA_NUM_ATTRIB_CONT_PORTS  21
#define VGA_NUM_DAC_ENTRIES       256

#define EXT_NUM_GRAPH_CONT_PORTS    0
#define EXT_NUM_SEQUENCER_PORTS     0
#define EXT_NUM_CRTC_PORTS          0
#define EXT_NUM_ATTRIB_CONT_PORTS   0
#define EXT_NUM_DAC_ENTRIES         0

//
// Minimal Attribute Controller Registers initialization command stream.
// Compatible EGA.
//
USHORT AT_Initialization[] =
{
    /* Reset ATC to index mode */
    IB,
    VGA_BASE_IO_PORT + ATT_INITIALIZE_PORT_COLOR /* INPUT_STATUS_1_COLOR */,

    /* Write the AC registers */
    METAOUT+ATCOUT,
    VGA_BASE_IO_PORT + ATT_ADDRESS_PORT /* ATT_DATA_WRITE_PORT */,
    16, 0,  // Values Count and Start Index
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, // Palette indices 0-5
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, // Palette indices 6-11
    0x0C, 0x0D, 0x0E, 0x0F,             // Palette indices 12-15

    /* Reset ATC to index mode */
    IB,
    VGA_BASE_IO_PORT + ATT_INITIALIZE_PORT_COLOR /* INPUT_STATUS_1_COLOR */,

    /* Enable screen and disable palette access */
    OB,
    VGA_BASE_IO_PORT + ATT_ADDRESS_PORT /* ATT_DATA_WRITE_PORT */,
    VIDEO_ENABLE,

    /* End of Stream */
    EOD
};

USHORT VGA_TEXT_0[] = {
    OWM,                            // begin setmode
    SEQ_ADDRESS_PORT,
    5,                              // count
    0x100,                          // start sync reset
    0x0101,0x0302,0x0003,0x0204,    // program up sequencer

    OB,                             // misc. register
    MISC_OUTPUT_REG_WRITE_PORT,
    0x67,

    OW,                             // text/graphics bit
    GRAPH_ADDRESS_PORT,
    0x0e06,
    
    OW,                             // end sync reset
    SEQ_ADDRESS_PORT,
    IND_SYNC_RESET,

    OB,
    SEQ_DATA_PORT,
    END_SYNC_RESET_VALUE,

    OW,                             // unprotect crtc 0-7
    CRTC_ADDRESS_PORT_COLOR,
    0xE11,                        

    METAOUT+INDXOUT,                // program crtc registers
    CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS,0,           // count, startindex
    0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,0x00,0x4F,0xD,0xE,0x0,0x0,0x0,0x0,
    0x9c,0x8E,0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,

    METAOUT+ATCOUT,                 // program atc registers
    ATT_ADDRESS_PORT,
    VGA_NUM_ATTRIB_CONT_PORTS,0,    // count, startindex
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
    0x17, 0x38, 0x39, 0x3A, 0x3B, 0x3C,
    0x3D, 0x3E, 0x3F, 0x3F, 0x04, 0x00,
    0x0F, 0x08, 0x00,

    METAOUT+INDXOUT,                // program gdc registers
    GRAPH_ADDRESS_PORT,
    VGA_NUM_GRAPH_CONT_PORTS,0,     // count, startindex
    0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x0E, 0x00, 0xFF,

    OB,
    DAC_PIXEL_MASK_PORT,
    0xFF,

    IB,                             // prepare atc for writing
    INPUT_STATUS_1_COLOR,
    
    OB,                             // turn video on.
    ATT_ADDRESS_PORT,
    VIDEO_ENABLE,

    EOD
};

USHORT VGA_640x480[] =
{
    /* Write the Sequencer Registers */
    OWM,
    VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT,
    VGA_NUM_SEQUENCER_PORTS,    // Values Count (5)
    // HI: Value in SEQ_DATA_PORT, LO: Register index in SEQ_ADDRESS_PORT
    0x0100, // Synchronous reset on
    0x0101, // 8-Dot Mode
    0x0F02, // Memory Plane Write Enable on all planes 0-3
    0x0003, // No character set selected
    0x0604, // Disable Odd/Even host mem addressing; Enable Extended Memory

    /* Write the Miscellaneous Register */
    OB,
    VGA_BASE_IO_PORT + MISC_OUTPUT_REG_WRITE_PORT,
    0xE3,   // V/H-SYNC polarity, Odd/Even High page select, RAM enable,
            // I/O Address select (1: color/graphics adapter)

    /* Enable Graphics Mode */
    OW,
    VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT,
    // HI: Value in GRAPH_DATA_PORT, LO: Register index in GRAPH_ADDRESS_PORT
    0x506,  // Select A0000h-AFFFFh memory region, Disable Alphanumeric mode

    /* Synchronous reset off */
    OW,
    VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT,
    // HI: Value in SEQ_DATA_PORT, LO: Register index in SEQ_ADDRESS_PORT
    0x0300, // Synchronous reset off (LO: IND_SYNC_RESET, HI: END_SYNC_RESET_VALUE)

    /* Unlock CRTC registers 0-7 */
    OW,
    VGA_BASE_IO_PORT + CRTC_ADDRESS_PORT_COLOR,
    0x511,

    /* Write the CRTC registers */
    METAOUT+INDXOUT,
    VGA_BASE_IO_PORT + CRTC_ADDRESS_PORT_COLOR,
    VGA_NUM_CRTC_PORTS, 0,              // Values Count (25) and Start Index
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0x0B, 0x3E, 0x00, 0x40, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xEA, 0x8C, 0xDF, 0x28, 0x00, 0xE7, 0x04, 0xE3,
    0xFF,

    /* Reset ATC to index mode */
    IB,
    VGA_BASE_IO_PORT + ATT_INITIALIZE_PORT_COLOR /* INPUT_STATUS_1_COLOR */,

    /* Write the AC registers */
    METAOUT+ATCOUT,
    VGA_BASE_IO_PORT + ATT_ADDRESS_PORT /* ATT_DATA_WRITE_PORT */,
    VGA_NUM_ATTRIB_CONT_PORTS, 0,       // Values Count (21) and Start Index
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, // Palette indices 0-5
    0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, // Palette indices 6-11
    0x0C, 0x0D, 0x0E, 0x0F,             // Palette indices 12-15
    0x01, 0x00, 0x0F, 0x00, 0x00,

    /* Write the GC registers */
    METAOUT+INDXOUT,
    VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT,
    VGA_NUM_GRAPH_CONT_PORTS, 0,        // Values Count (9) and Start Index
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x05, 0x0F, 0xFF,

    /* Set the PEL mask */
    OB,
    VGA_BASE_IO_PORT + DAC_PIXEL_MASK_PORT,
    0xFF,

    /* Reset ATC to index mode */
    IB,
    VGA_BASE_IO_PORT + ATT_INITIALIZE_PORT_COLOR /* INPUT_STATUS_1_COLOR */,

    /* Enable screen and disable palette access */
    OB,
    VGA_BASE_IO_PORT + ATT_ADDRESS_PORT /* ATT_DATA_WRITE_PORT */,
    VIDEO_ENABLE,

    /* End of Stream */
    EOD
};

#define __inpw(x) __inword(x)
#define __inpb(x) __inbyte(x)
#define __outpw(x, y) __outword(x, y)
#define __outpb(x, y) __outbyte(x, y)

ULONG_PTR VgaBase;
ULONG_PTR VgaRegisterBase;

static BOOLEAN
NTAPI
VgaInterpretCmdStream(
    PUSHORT CmdStream)
{
    USHORT Cmd;
    UCHAR Major, Minor;
    USHORT Port;
    USHORT Count;
    UCHAR Index;
    UCHAR Value;
    USHORT ShortValue;

    /* First make sure that we have a Command Stream */
    if (!CmdStream) return TRUE;

    /* Loop as long as we have commands */
    while (*CmdStream != EOD)
    {
        /* Get the next command and its Major and Minor functions */
        Cmd = *CmdStream++;
        Major = Cmd & 0xF0;
        Minor = Cmd & 0x0F;

        /* Check which major function this is */
        if (Major == INOUT)
        {
            /* Check the minor function */
            if (Minor & IO /* CMD_STREAM_READ */)
            {
                /* Check the sub-type */
                if (Minor & BW /* CMD_STREAM_USHORT */)
                {
                    /* Get the port and read an USHORT from it */
                    Port = *CmdStream++;
                    ShortValue = __inpw(Port);
                }
                else // if (Minor & CMD_STREAM_WRITE)
                {
                    /* Get the port and read an UCHAR from it */
                    Port = *CmdStream++;
                    Value = __inpb(Port);
                }
            }
            else if (Minor & MULTI /* CMD_STREAM_WRITE_ARRAY */)
            {
                /* Check the sub-type */
                if (Minor & BW /* CMD_STREAM_USHORT */)
                {
                    /* Get the port and the count of elements */
                    Port = *CmdStream++;
                    Count = *CmdStream++;

                    /* Write the USHORT to the port; the buffer is what's in the command stream */
                    WRITE_PORT_BUFFER_USHORT((PUSHORT)(VgaRegisterBase + Port), CmdStream, Count);

                    /* Move past the buffer in the command stream */
                    CmdStream += Count;
                }
                else // if (Minor & CMD_STREAM_WRITE)
                {
                    /* Get the port and the count of elements */
                    Port = *CmdStream++;
                    Count = *CmdStream++;

                    /* Loop the command array */
                    for (; Count; --Count, ++CmdStream)
                    {
                        /* Get the UCHAR and write it to the port */
                        Value = (UCHAR)*CmdStream;
                        __outpb(Port, Value);
                    }
                }
            }
            else if (Minor & BW /* CMD_STREAM_USHORT */)
            {
                /* Get the port */
                Port = *CmdStream++;

                /* Get the USHORT and write it to the port */
                ShortValue = *CmdStream++;
                __outpw(Port, ShortValue);
            }
            else // if (Minor & CMD_STREAM_WRITE)
            {
                /* Get the port */
                Port = *CmdStream++;

                /* Get the UCHAR and write it to the port */
                Value = (UCHAR)*CmdStream++;
                __outpb(Port, Value);
            }
        }
        else if (Major == METAOUT)
        {
            /* Check the minor function. Note these are not flags. */
            switch (Minor)
            {
                case INDXOUT:
                {
                    /* Get the port, the count of elements and the start index */
                    Port = *CmdStream++;
                    Count = *CmdStream++;
                    Index = (UCHAR)*CmdStream++;

                    /* Loop the command array */
                    for (; Count; --Count, ++Index, ++CmdStream)
                    {
                        /* Get the USHORT and write it to the port */
                        ShortValue = (USHORT)Index + ((*CmdStream) << 8);
                        __outpw(Port, ShortValue);
                    }
                    break;
                }

                case ATCOUT:
                {
                    /* Get the port, the count of elements and the start index */
                    Port = *CmdStream++;
                    Count = *CmdStream++;
                    Index = (UCHAR)*CmdStream++;

                    /* Loop the command array */
                    for (; Count; --Count, ++Index, ++CmdStream)
                    {
                        /* Write the index */
                        __outpb(Port, Index);

                        /* Get the UCHAR and write it to the port */
                        Value = (UCHAR)*CmdStream;
                        __outpb(Port, Value);
                    }
                    break;
                }

                case MASKOUT:
                {
                    /* Get the port */
                    Port = *CmdStream++;

                    /* Read the current value and add the stream data */
                    Value = __inpb(Port);
                    Value &= *CmdStream++;
                    Value ^= *CmdStream++;

                    /* Write the value */
                    __outpb(Port, Value);
                    break;
                }

                default:
                    /* Unknown command, fail */
                    return FALSE;
            }
        }
        else if (Major != NCMD)
        {
            /* Unknown major function, fail */
            return FALSE;
        }
    }

    /* If we got here, return success */
    return TRUE;
}

static BOOLEAN
NTAPI
VgaIsPresent(VOID)
{
    UCHAR OrgGCAddr, OrgReadMap, OrgBitMask;
    UCHAR OrgSCAddr, OrgMemMode;
    UCHAR i;

    /* Remember the original state of the Graphics Controller Address register */
    OrgGCAddr = __inpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT);

    /*
     * Write the Read Map register with a known state so we can verify
     * that it isn't changed after we fool with the Bit Mask. This ensures
     * that we're dealing with indexed registers, since both the Read Map and
     * the Bit Mask are addressed at GRAPH_DATA_PORT.
     */
    __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_READ_MAP);

    /*
     * If we can't read back the Graphics Address register setting we just
     * performed, it's not readable and this isn't a VGA.
     */
    if ((__inpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT) & GRAPH_ADDR_MASK) != IND_READ_MAP)
        return FALSE;

    /*
     * Set the Read Map register to a known state.
     */
    OrgReadMap = __inpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT);
    __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, READ_MAP_TEST_SETTING);

    /* Read it back... it should be the same */
    if (__inpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT) != READ_MAP_TEST_SETTING)
    {
        /*
         * The Read Map setting we just performed can't be read back; not a
         * VGA. Restore the default Read Map state and fail.
         */
        __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, READ_MAP_DEFAULT);
        return FALSE;
    }

    /* Remember the original setting of the Bit Mask register */
    __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_BIT_MASK);

    /* Read it back... it should be the same */
    if ((__inpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT) & GRAPH_ADDR_MASK) != IND_BIT_MASK)
    {
        /*
         * The Graphics Address register setting we just made can't be read
         * back; not a VGA. Restore the default Read Map state and fail.
         */
        __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_READ_MAP);
        __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, READ_MAP_DEFAULT);
        return FALSE;
    }

    /* Read the VGA Data Register */
    OrgBitMask = __inpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT);

    /*
     * Set up the initial test mask we'll write to and read from the Bit Mask,
     * and loop on the bitmasks.
     */
    for (i = 0xBB; i; i >>= 1)
    {
        /* Write the test mask to the Bit Mask */
        __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, i);

        /* Read it back... it should be the same */
        if (__inpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT) != i)
        {
            /*
             * The Bit Mask is not properly writable and readable; not a VGA.
             * Restore the Bit Mask and Read Map to their default states and fail.
             */
            __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, BIT_MASK_DEFAULT);
            __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_READ_MAP);
            __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, READ_MAP_DEFAULT);
            return FALSE;
        }
    }

    /*
     * There's something readable at GRAPH_DATA_PORT; now switch back and
     * make sure that the Read Map register hasn't changed, to verify that
     * we're dealing with indexed registers.
     */
    __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_READ_MAP);

    /* Read it back */
    if (__inpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT) != READ_MAP_TEST_SETTING)
    {
        /*
         * The Read Map is not properly writable and readable; not a VGA.
         * Restore the Bit Mask and Read Map to their default states, in case
         * this is an EGA, so subsequent writes to the screen aren't garbled.
         * Then fail.
         */
        __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, READ_MAP_DEFAULT);
        __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_BIT_MASK);
        __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, BIT_MASK_DEFAULT);
        return FALSE;
    }

    /*
     * We've pretty surely verified the existence of the Bit Mask register.
     * Put the Graphics Controller back to the original state.
     */
    __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, OrgReadMap);
    __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, IND_BIT_MASK);
    __outpb(VGA_BASE_IO_PORT + GRAPH_DATA_PORT, OrgBitMask);
    __outpb(VGA_BASE_IO_PORT + GRAPH_ADDRESS_PORT, OrgGCAddr);

    /*
     * Now, check for the existence of the Chain4 bit.
     */

    /*
     * Remember the original states of the Sequencer Address and Memory Mode
     * registers.
     */
    OrgSCAddr = __inpb(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT);
    __outpb(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, IND_MEMORY_MODE);

    /* Read it back... it should be the same */
    if ((__inpb(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT) & SEQ_ADDR_MASK) != IND_MEMORY_MODE)
    {
        /*
         * Couldn't read back the Sequencer Address register setting
         * we just performed, fail.
         */
        return FALSE;
    }

    /* Read sequencer Data */
    OrgMemMode = __inpb(VGA_BASE_IO_PORT + SEQ_DATA_PORT);

    /*
     * Toggle the Chain4 bit and read back the result. This must be done during
     * sync reset, since we're changing the chaining state.
     */

    /* Begin sync reset */
    __outpw(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, (IND_SYNC_RESET + (START_SYNC_RESET_VALUE << 8)));

    /* Toggle the Chain4 bit */
    __outpb(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, IND_MEMORY_MODE);
    __outpb(VGA_BASE_IO_PORT + SEQ_DATA_PORT, OrgMemMode ^ CHAIN4_MASK);

    /* Read it back... it should be the same */
    if (__inpb(VGA_BASE_IO_PORT + SEQ_DATA_PORT) != (OrgMemMode ^ CHAIN4_MASK))
    {
        /*
         * Chain4 bit is not there, not a VGA.
         * Set text mode default for Memory Mode register.
         */
        __outpb(VGA_BASE_IO_PORT + SEQ_DATA_PORT, MEMORY_MODE_TEXT_DEFAULT);

        /* End sync reset */
        __outpw(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, (IND_SYNC_RESET + (END_SYNC_RESET_VALUE << 8)));

        /* Fail */
        return FALSE;
    }

    /*
     * It's a VGA.
     */

    /* Restore the original Memory Mode setting */
    __outpb(VGA_BASE_IO_PORT + SEQ_DATA_PORT, OrgMemMode);

    /* End sync reset */
    __outpw(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, (IND_SYNC_RESET + (END_SYNC_RESET_VALUE << 8)));

    /* Restore the original Sequencer Address setting */
    __outpb(VGA_BASE_IO_PORT + SEQ_ADDRESS_PORT, OrgSCAddr);

    /* VGA is present! */
    return TRUE;
}

static int VidpCurrentX, VidpCurrentY;

/* PUBLIC FUNCTIONS **********************************************************/

/*
 * @implemented
 */
BOOLEAN
NTAPI
VidInitialize(
    BOOLEAN SetMode)
{
    ULONG_PTR Context = 0;
    PHYSICAL_ADDRESS TranslatedAddress;
    PHYSICAL_ADDRESS NullAddress = {{0, 0}}, VgaAddress;
    ULONG AddressSpace;
    BOOLEAN Result;
    ULONG_PTR Base;

    /* Make sure that we have a bus translation function */
    if (!HalPrivateDispatchTable.HalFindBusAddressTranslation) return FALSE;

    /* Loop trying to find possible VGA base addresses */
    while (TRUE)
    {
        /* Get the VGA Register address */
        AddressSpace = 1;
        Result = HalPrivateDispatchTable.HalFindBusAddressTranslation(NullAddress,
                                              &AddressSpace,
                                              &TranslatedAddress,
                                              &Context,
                                              TRUE);
        if (!Result) return FALSE;

        /* See if this is I/O Space, which we need to map */
        if (!AddressSpace)
        {
            /* Map it */
            Base = (ULONG_PTR)MmMapIoSpace(TranslatedAddress, 0x400, MmNonCached);
        }
        else
        {
            /* The base is the translated address, no need to map I/O space */
            Base = TranslatedAddress.LowPart;
        }

        /* Try to see if this is VGA */
        VgaRegisterBase = Base;
        if (VgaIsPresent())
        {
            /* Translate the VGA Memory Address */
            VgaAddress.LowPart = MEM_VGA;
            VgaAddress.HighPart = 0;
            AddressSpace = 0;
            Result = HalPrivateDispatchTable.HalFindBusAddressTranslation(VgaAddress,
                                                  &AddressSpace,
                                                  &TranslatedAddress,
                                                  &Context,
                                                  FALSE);
            if (Result) break;
        }
        else
        {
            /* It's not, so unmap the I/O space if we mapped it */
            if (!AddressSpace) MmUnmapIoSpace((PVOID)VgaRegisterBase, 0x400);
        }

        /* Continue trying to see if there's any other address */
    }

    /* Success! See if this is I/O Space, which we need to map */
    if (!AddressSpace)
    {
        /* Map it */
        Base = (ULONG_PTR)MmMapIoSpace(TranslatedAddress,
                                       MEM_VGA_SIZE,
                                       MmNonCached);
    }
    else
    {
        /* The base is the translated address, no need to map I/O space */
        Base = TranslatedAddress.LowPart;
    }

    /* Set the VGA Memory Base */
    VgaBase = Base;

    /* Now check if we have to set the mode */
    if (SetMode)
    {
        /* Clear the current position */
        VidpCurrentX = 0;
        VidpCurrentY = 0;

        /* Reset the display and initialize it */
        if (HalPrivateDispatchTable.HalResetDisplay())
        {
            /* The HAL handled the display, re-initialize only the AC registers */
            VgaInterpretCmdStream(AT_Initialization);
        }
        else
        {
            /* The HAL didn't handle the display, fully re-initialize the VGA */
            VgaInterpretCmdStream(VGA_640x480);
        }
    }

    /* VGA is ready */
    return TRUE;
}

static const unsigned char default_vga_palette[256 * 3] = {
  0,   0,   0,    0,   0, 170,    0, 170,   0,    0, 170, 170,
170,   0,   0,  170,   0, 170,  170,  85,   0,  170, 170, 170,
 85,  85,  85,   85,  85, 255,   85, 255,  85,   85, 255, 255,
255,  85,  85,  255,  85, 255,  255, 255,  85,  255, 255, 255,
  0,   0,   0,   20,  20,  20,   32,  32,  32,   44,  44,  44,
 56,  56,  56,   69,  69,  69,   81,  81,  81,   97,  97,  97,
113, 113, 113,  130, 130, 130,  146, 146, 146,  162, 162, 162,
182, 182, 182,  203, 203, 203,  227, 227, 227,  255, 255, 255,
  0,   0, 255,   65,   0, 255,  125,   0, 255,  190,   0, 255,
255,   0, 255,  255,   0, 190,  255,   0, 125,  255,   0,  65,
255,   0,   0,  255,  65,   0,  255, 125,   0,  255, 190,   0,
255, 255,   0,  190, 255,   0,  125, 255,   0,   65, 255,   0,
  0, 255,   0,    0, 255,  65,    0, 255, 125,    0, 255, 190,
  0, 255, 255,    0, 190, 255,    0, 125, 255,    0,  65, 255,
125, 125, 255,  158, 125, 255,  190, 125, 255,  223, 125, 255,
255, 125, 255,  255, 125, 223,  255, 125, 190,  255, 125, 158,
255, 125, 125,  255, 158, 125,  255, 190, 125,  255, 223, 125,
255, 255, 125,  223, 255, 125,  190, 255, 125,  158, 255, 125,
125, 255, 125,  125, 255, 158,  125, 255, 190,  125, 255, 223,
125, 255, 255,  125, 223, 255,  125, 190, 255,  125, 158, 255,
182, 182, 255,  199, 182, 255,  219, 182, 255,  235, 182, 255,
255, 182, 255,  255, 182, 235,  255, 182, 219,  255, 182, 199,
255, 182, 182,  255, 199, 182,  255, 219, 182,  255, 235, 182,
255, 255, 182,  235, 255, 182,  219, 255, 182,  199, 255, 182,
182, 255, 182,  182, 255, 199,  182, 255, 219,  182, 255, 235,
182, 255, 255,  182, 235, 255,  182, 219, 255,  182, 199, 255,
  0,   0, 113,   28,   0, 113,   56,   0, 113,   85,   0, 113,
113,   0, 113,  113,   0,  85,  113,   0,  56,  113,   0,  28,
113,   0,   0,  113,  28,   0,  113,  56,   0,  113,  85,   0,
113, 113,   0,   85, 113,   0,   56, 113,   0,   28, 113,   0,
  0, 113,   0,    0, 113,  28,    0, 113,  56,    0, 113,  85,
  0, 113, 113,    0,  85, 113,    0,  56, 113,    0,  28, 113,
 56,  56, 113,   69,  56, 113,   85,  56, 113,   97,  56, 113,
113,  56, 113,  113,  56,  97,  113,  56,  85,  113,  56,  69,
113,  56,  56,  113,  69,  56,  113,  85,  56,  113,  97,  56,
113, 113,  56,   97, 113,  56,   85, 113,  56,   69, 113,  56,
 56, 113,  56,   56, 113,  69,   56, 113,  85,   56, 113,  97,
 56, 113, 113,   56,  97, 113,   56,  85, 113,   56,  69, 113,
 81,  81, 113,   89,  81, 113,   97,  81, 113,  105,  81, 113,
113,  81, 113,  113,  81, 105,  113,  81,  97,  113,  81,  89,
113,  81,  81,  113,  89,  81,  113,  97,  81,  113, 105,  81,
113, 113,  81,  105, 113,  81,   97, 113,  81,   89, 113,  81,
 81, 113,  81,   81, 113,  89,   81, 113,  97,   81, 113, 105,
 81, 113, 113,   81, 105, 113,   81,  97, 113,   81,  89, 113,
  0,   0,  65,   16,   0,  65,   32,   0,  65,   48,   0,  65,
 65,   0,  65,   65,   0,  48,   65,   0,  32,   65,   0,  16,
 65,   0,   0,   65,  16,   0,   65,  32,   0,   65,  48,   0,
 65,  65,   0,   48,  65,   0,   32,  65,   0,   16,  65,   0,
  0,  65,   0,    0,  65,  16,    0,  65,  32,    0,  65,  48,
  0,  65,  65,    0,  48,  65,    0,  32,  65,    0,  16,  65,
 32,  32,  65,   40,  32,  65,   48,  32,  65,   56,  32,  65,
 65,  32,  65,   65,  32,  56,   65,  32,  48,   65,  32,  40,
 65,  32,  32,   65,  40,  32,   65,  48,  32,   65,  56,  32,
 65,  65,  32,   56,  65,  32,   48,  65,  32,   40,  65,  32,
 32,  65,  32,   32,  65,  40,   32,  65,  48,   32,  65,  56,
 32,  65,  65,   32,  56,  65,   32,  48,  65,   32,  40,  65,
 44,  44,  65,   48,  44,  65,   52,  44,  65,   60,  44,  65,
 65,  44,  65,   65,  44,  60,   65,  44,  52,   65,  44,  48,
 65,  44,  44,   65,  48,  44,   65,  52,  44,   65,  60,  44,
 65,  65,  44,   60,  65,  44,   52,  65,  44,   48,  65,  44,
 44,  65,  44,   44,  65,  48,   44,  65,  52,   44,  65,  60,
 44,  65,  65,   44,  60,  65,   44,  52,  65,   44,  48,  65,
  0,   0,   0,    0,   0,   0,    0,   0,   0,    0,   0,   0,
  0,   0,   0,    0,   0,   0,    0,   0,   0,    0,   0,   0
};

//
// vga mode switcher by Jonas Berlin -98 <jberlin@cc.hut.fi>
//


typedef char byte;
typedef unsigned short word;
typedef unsigned long dword;

#define SZ(x) (sizeof(x)/sizeof(x[0]))


// misc out (3c2h) value for various modes

#define R_COM  0x63 // "common" bits

#define R_W256 0x00
#define R_W320 0x00
#define R_W360 0x04
#define R_W376 0x04
#define R_W400 0x04

#define R_H200 0x00
#define R_H224 0x80
#define R_H240 0x80
#define R_H256 0x80
#define R_H270 0x80
#define R_H300 0x80
#define R_H360 0x00
#define R_H400 0x00
#define R_H480 0x80
#define R_H564 0x80
#define R_H600 0x80

typedef struct tagRGBQUAD
{
    UCHAR rgbBlue;
    UCHAR rgbGreen;
    UCHAR rgbRed;
    UCHAR rgbReserved;
} RGBQUAD, *LPRGBQUAD;

VOID
NTAPI
SetPaletteEntryRGB(
    ULONG Id,
    RGBQUAD Rgb)
{
    /* Set the palette index */
    __outpb(VGA_BASE_IO_PORT + DAC_ADDRESS_WRITE_PORT, (UCHAR)Id);

    /* Set RGB colors */
    __outpb(VGA_BASE_IO_PORT + DAC_DATA_REG_PORT, Rgb.rgbRed >> 2);
    __outpb(VGA_BASE_IO_PORT + DAC_DATA_REG_PORT, Rgb.rgbGreen >> 2);
    __outpb(VGA_BASE_IO_PORT + DAC_DATA_REG_PORT, Rgb.rgbBlue >> 2);
}

static const byte hor_regs [] = { 0x0,  0x1,  0x2,  0x3,  0x4, 
0x5,  0x13 };

static const byte width_256[] = { 0x5f, 0x3f, 0x40, 0x82, 0x4a,
0x9a, 0x20 };
static const byte width_320[] = { 0x5f, 0x4f, 0x50, 0x82, 0x54,
0x80, 0x28 };
static const byte width_360[] = { 0x6b, 0x59, 0x5a, 0x8e, 0x5e,
0x8a, 0x2d };
static const byte width_376[] = { 0x6e, 0x5d, 0x5e, 0x91, 0x62,
0x8f, 0x2f };
static const byte width_400[] = { 0x70, 0x63, 0x64, 0x92, 0x65,
0x82, 0x32 };

static const byte ver_regs  [] = { 0x6,  0x7,  0x9,  0x10, 0x11,
0x12, 0x15, 0x16 };

static const byte height_200[] = { 0xbf, 0x1f, 0x41, 0x9c, 0x8e,
0x8f, 0x96, 0xb9 };
static const byte height_224[] = { 0x0b, 0x3e, 0x41, 0xda, 0x9c,
0xbf, 0xc7, 0x04 };
static const byte height_240[] = { 0x0d, 0x3e, 0x41, 0xea, 0xac,
0xdf, 0xe7, 0x06 };
static const byte height_256[] = { 0x23, 0xb2, 0x61, 0x0a, 0xac,
0xff, 0x07, 0x1a };
static const byte height_270[] = { 0x30, 0xf0, 0x61, 0x20, 0xa9,
0x1b, 0x1f, 0x2f };
static const byte height_300[] = { 0x70, 0xf0, 0x61, 0x5b, 0x8c,
0x57, 0x58, 0x70 };
static const byte height_360[] = { 0xbf, 0x1f, 0x40, 0x88, 0x85,
0x67, 0x6d, 0xba };
static const byte height_400[] = { 0xbf, 0x1f, 0x40, 0x9c, 0x8e,
0x8f, 0x96, 0xb9 };
static const byte height_480[] = { 0x0d, 0x3e, 0x40, 0xea, 0xac,
0xdf, 0xe7, 0x06 };
static const byte height_564[] = { 0x62, 0xf0, 0x60, 0x37, 0x89,
0x33, 0x3c, 0x5c };
static const byte height_600[] = { 0x70, 0xf0, 0x60, 0x5b, 0x8c,
0x57, 0x58, 0x70 };

// the chain4 parameter should be 1 for normal 13h-type mode, but
// only allows 320x200 256x200, 256x240 and 256x256 because you
// can only access the first 64kb

// if chain4 is 0, then plane mode is used (tweaked modes), and
// you'll need to switch planes to access the whole screen but
// that allows you using any resolution, up to 400x600

int init_graph_vga(int width, int height,int chain4) 
  // returns 1=ok, 0=fail
{
   const byte *w,*h;
   byte val;
   int a, i;

   switch(width) {
      case 256: w=width_256; val=R_COM+R_W256; break;
      case 320: w=width_320; val=R_COM+R_W320; break;
      case 360: w=width_360; val=R_COM+R_W360; break;
      case 376: w=width_376; val=R_COM+R_W376; break;
      case 400: w=width_400; val=R_COM+R_W400; break;
      default: return 0; // fail
   }
   switch(height) {
      case 200: h=height_200; val|=R_H200; break;
      case 224: h=height_224; val|=R_H224; break;
      case 240: h=height_240; val|=R_H240; break;
      case 256: h=height_256; val|=R_H256; break;
      case 270: h=height_270; val|=R_H270; break;
      case 300: h=height_300; val|=R_H300; break;
      case 360: h=height_360; val|=R_H360; break;
      case 400: h=height_400; val|=R_H400; break;
      case 480: h=height_480; val|=R_H480; break;
      case 564: h=height_564; val|=R_H564; break;
      case 600: h=height_600; val|=R_H600; break;
      default: return 0; // fail
   }

   // chain4 not available if mode takes over 64k

   if(chain4 && (long)width*(long)height>65536L) return 0; 

   // here goes the actual modeswitch

   __outpb(0x3c2,val);
   __outpw(0x3d4,0x0e11); // enable regs 0-7

   for(a=0;a<SZ(hor_regs);++a) 
      __outpw(0x3d4,(word)((w[a]<<8)+hor_regs[a]));
   for(a=0;a<SZ(ver_regs);++a)
      __outpw(0x3d4,(word)((h[a]<<8)+ver_regs[a]));

   __outpw(0x3d4,0x0008); // vert.panning = 0

   if(chain4) {
      __outpw(0x3d4,0x4014);
      __outpw(0x3d4,0xa317);
      __outpw(0x3c4,0x0e04);
   } else {
      __outpw(0x3d4,0x0014);
      __outpw(0x3d4,0xe317);
      __outpw(0x3c4,0x0604);
   }

   __outpw(0x3c4,0x0101);
   __outpw(0x3c4,0x0f02); // enable writing to all planes
   __outpw(0x3ce,0x4005); // 256color mode
   //__outpw(0x3ce,0x0506); // graph mode & A000-AFFF

   __inpb(0x3da);
   __outpb(0x3c0,0x30); __outpb(0x3c0,0x41);
   __outpb(0x3c0,0x33); __outpb(0x3c0,0x00);

   for(a=0;a<16;a++) {    // ega pal
      __outpb(0x3c0,(byte)a); 
      __outpb(0x3c0,(byte)a); 
   } 
   
   __outpb(0x3c0, 0x20); // enable video

   for (i = 0; i < 256; i++) {
    RGBQUAD Rgb;
    Rgb.rgbRed = default_vga_palette[i * 3];
    Rgb.rgbGreen = default_vga_palette[(i * 3) + 1];
    Rgb.rgbBlue = default_vga_palette[(i * 3) + 2];
    SetPaletteEntryRGB(i, Rgb);
   }

   return 1;
}

