using System;
using System.Collections.Generic;
using System.IO;
using Unity.Collections;
using UnityEngine.Profiling;
using static Constants;

public class NuflixFormat
{
    private LayeredImage _image;

    public int WaitAddress { get; private set; }

    public LayeredImage Image => _image;

    private NativeArray<bool> _bitmap;
    private NativeArray<int> _bitmapColors;

    private NativeArray<bool> _underlay;
    private NativeArray<int> _underlayColors;

    private NativeArray<bool> _bug;
    private NativeArray<int> _bugColors;

    private NativeArray<int> _borderColors;

    private static readonly ushort[] ScreenRamRowOffsets =
    {
        0x3c00, 0x3800, 0x3400, 0x3000, 0x2c28, 0x2828, 0x2428, 0x2028,
        0x3c50, 0x3850, 0x3450, 0x3050, 0x2c78, 0x2878, 0x2478, 0x2078,
        0x3ca0, 0x38a0, 0x34a0, 0x30a0, 0x2cc8, 0x28c8, 0x24c8, 0x20c8,
        0x3cf0, 0x38f0, 0x34f0, 0x30f0, 0x2d18, 0x2918, 0x2518, 0x2118,
        0x3d40, 0x3940, 0x3540, 0x3140, 0x2d68, 0x2968, 0x2568, 0x2168,
        0x3d90, 0x3990, 0x3590, 0x3190, 0x2db8, 0x29b8, 0x25b8, 0x21b8,
        0x3de0, 0x39e0, 0x35e0, 0x31e0, 0x2e08, 0x2a08, 0x2608, 0x2208,
        0x3e30, 0x3a30, 0x3630, 0x3230, 0x2e58, 0x2a58, 0x2658, 0x2258,
        0x0280, 0x0680, 0x0a80, 0x0e80,
        0x02a8, 0x06a8, 0x0aa8, 0x0ea8,
        0x02d0, 0x06d0, 0x0ad0, 0x0ed0,
        0x02f8, 0x06f8, 0x0af8, 0x0ef8,
        0x0320, 0x0720, 0x0b20, 0x0f20,
        0x0748, 0x0b48, 0x0f48, 0x0348,
        0x0770, 0x0b70, 0x0f70, 0x0370,
        0x0798, 0x0b98, 0x0f98, 0x0398,
        0x07c0, 0x0bc0, 0x0fc0, 0x03c0
    };

    private static readonly ushort[] BugHiresSpriteRowOffsets =
    {
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x59c0, 0x5980, 0x5940, 0x5900, 0x58c0, 0x5880, 0x5840, 0x5800,
        0x0800, 0x0840, 0x0880, 0x08c0,
        0x0800, 0x0840, 0x0880, 0x08c0,
        0x0800, 0x0840, 0x0880, 0x08c0,
        0x0800, 0x0840, 0x0880, 0x08c0,
        0x0800, 0x0840, 0x0880, 0x08c0,
        0x0840, 0x0880, 0x08c0, 0x0800,
        0x0840, 0x0880, 0x08c0, 0x0800,
        0x0840, 0x0880, 0x08c0, 0x0800,
        0x0840, 0x0880, 0x08c0, 0x0800
    };

    private static readonly ushort[] BugMultiSpriteRowOffsets =
    {
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x57c0, 0x5780, 0x5740, 0x5700, 0x56c0, 0x5680, 0x5640, 0x5600,
        0x0400, 0x0440, 0x0480, 0x04c0,
        0x0400, 0x0440, 0x0480, 0x04c0,
        0x0400, 0x0440, 0x0480, 0x04c0,
        0x0400, 0x0440, 0x0480, 0x04c0,
        0x0400, 0x0440, 0x0480, 0x04c0,
        0x0440, 0x0480, 0x04c0, 0x0400,
        0x0440, 0x0480, 0x04c0, 0x0400,
        0x0440, 0x0480, 0x04c0, 0x0400,
        0x0440, 0x0480, 0x04c0, 0x0400
    };

    private static readonly ushort[] UnderlayRowOffsets =
    {
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x3e80, 0x3a80, 0x3680, 0x3280, 0x2e80, 0x2a80, 0x2680, 0x2280,
        0x0100, 0x0500, 0x0900, 0x0d00,
        0x0100, 0x0500, 0x0900, 0x0d00,
        0x0100, 0x0500, 0x0900, 0x0d00,
        0x0100, 0x0500, 0x0900, 0x0d00,
        0x0100, 0x0500, 0x0900, 0x0d00,
        0x0500, 0x0900, 0x0d00, 0x0100,
        0x0500, 0x0900, 0x0d00, 0x0100,
        0x0500, 0x0900, 0x0d00, 0x0100,
        0x0500, 0x0900, 0x0d00, 0x0100
    };

    private byte GetByte(NativeArray<bool> bitmap, int offset)
    {
        byte result = 0;
        for (var i = 0; i < 8; i++)
        {
            if (bitmap[offset + i])
            {
                result |= (byte)(0x80 >> i);
            }
        }
        return result;
    }

    private void SetByte(NativeArray<bool> bitmap, int offset, byte value)
    {
        for (var i = 0; i < 8; i++)
        {
            bitmap[offset + i] = (value & (0x80 >> i)) != 0;
        }
    }

    public NuflixExportResult Export(LayeredImage image, bool ntsc, string logPath = null)
    {
        Profiler.BeginSample("Export");

        Profiler.BeginSample("Clone");
        _image = image.Clone();
        _bitmap = _image.Bitmap;
        _bitmapColors = _image.BitmapColors;
        _underlay = _image.Underlay;
        _underlayColors = _image.UnderlayColors;
        _bug = _image.Bug;
        _bugColors = _image.BugColors;
        _borderColors = _image.BorderColors;
        Profiler.EndSample();

        // We fill in the unused entries with the upcoming colour, but marking them as deferrable
        Profiler.BeginSample("FillColors");
        for (var i = _underlayColors.Length - 1 - UnderlayColumns; i >= 0; i--)
        {
            if (_underlayColors[i] < 0)
            {
                _underlayColors[i] = LayeredImage.UnusedColor(_underlayColors[i + UnderlayColumns]);
            }
        }

        var bugColorsUsed = new bool[BugColorSlots];
        for (var y = AttributeHeight - 2; y >= 0; y--)
        {
            _image.GetBugColorsUsed(y, bugColorsUsed);
            for (var i = 0; i < BugColorSlots; i++)
            {
                if (!bugColorsUsed[i])
                {
                    _bugColors[y * BugColorSlots + i] = LayeredImage.UnusedColor(_bugColors[(y + 1) * BugColorSlots + i]);
                }
            }
        }
        Profiler.EndSample();

        var result = new List<byte>(File.ReadAllBytes($"{MainWindowLogic.SettingsDir}/nufli-template.bin"));
        WaitAddress = result[0] | (result[1] << 8);
        var bankChangeAddressPointer = (result[2] | (result[3] << 8)) - 0x2000;
        var initialXPointerPal = (result[24] | (result[25] << 8)) - 0x2000;
        var initialYPointerPal = (result[26] | (result[27] << 8)) - 0x2000;
        var initialXPointerNtsc = (result[48] | (result[49] << 8)) - 0x2000;
        var initialYPointerNtsc = (result[50] | (result[51] << 8)) - 0x2000;
        var speedCodeFixCallPointer = (result[52] | (result[53] << 8)) - 0x2000;
        var topBorderColorPointer = (result[54] | (result[55] << 8)) - 0x2000;
        var topBackgroundColorPointer = (result[56] | (result[57] << 8)) - 0x2000;

        for (var i = 2; i < 12; i++)
        {
            var pointerPal = (result[i << 1] | (result[(i << 1) + 1] << 8)) - 0x2000;
            var pointerNtsc = (result[(i + 12) << 1] | (result[((i + 12) << 1) + 1] << 8)) - 0x2000;
            switch (i)
            {
                case 2: // d025
                    result[pointerPal] = result[pointerNtsc] = (byte)_bugColors[1];
                    break;
                case 3: // d026
                    result[pointerPal] = result[pointerNtsc] = (byte)_bugColors[3];
                    break;
                case 4: // d027
                    result[pointerPal] = result[pointerNtsc] = (byte)_bugColors[0];
                    break;
                case 11: // d02e
                    result[pointerPal] = result[pointerNtsc] = (byte)_bugColors[2];
                    break;
                default: // d028-2d
                    result[pointerPal] = result[pointerNtsc] = (byte)_underlayColors[i - 5];
                    break;
            }
        }

        result[topBorderColorPointer] = (byte)Image.BorderColors[0];
        result[topBackgroundColorPointer] = (byte)Image.TopBackgroundColor;

        // Speedcode
        var codeGeneration = new CodeGeneration(Image);
        Profiler.BeginSample("FindUpdates");
        var (updates, deferredUpdates) = FindRegisterUpdates();
        Profiler.EndSample();
        var slowValuesMapping = new Dictionary<int, byte>();
        for (var i = 0; i < 0x20; i++)
        {
            slowValuesMapping[result[0x2140 + i]] = (byte)(i + 0xd1);
        }
        Profiler.BeginSample("CodeGen");
        codeGeneration.Generate(updates, deferredUpdates, slowValuesMapping, logPath);
        Profiler.EndSample();
        result[initialXPointerPal] = result[initialXPointerNtsc] = codeGeneration.InitX;
        result[initialYPointerPal] = result[initialYPointerNtsc] = codeGeneration.InitY;
        result[bankChangeAddressPointer] = (byte)codeGeneration.BankChangeAddress;
        result[bankChangeAddressPointer + 1] = (byte)(codeGeneration.BankChangeAddress >> 8);
        if (ntsc)
        {
            // Remove call for adjusting the code for NTSC at runtime, because we'll do it below
            result[speedCodeFixCallPointer] = 0x2c;
        }

        // Regenerate image data after adjusting colours for the limitations of the speedcode when converting
        if (image.DirectlyEditing)
        {
            Profiler.BeginSample("RebuildReference");
            image.RenderLayers(_image.ReferencePixels);
            Profiler.EndSample();
        }
        Profiler.BeginSample("GenerateLayers");
        _image.GenerateLayers();
        Profiler.EndSample();

        Profiler.BeginSample("BuildOutput");
        // Bitmap
        for (var i = 0; i < 8000; i++)
        {
            var ofs = i % ScreenWidth;
            var y = ((i / ScreenWidth) << 3) + (ofs & 7);
            var x = ofs & ~7;
            result[i < 0x1400 ? 0x4000 + i : i] = GetByte(_bitmap, y * ScreenWidth + x);
        }

        // Shared bitmap row
        for (var i = 0; i < AttributeWidth; i++)
        {
            result[0x12c7 + (i << 3)] = result[0x52c7 + (i << 3)];
        }

        // Attributes
        for (var y = 0; y < AttributeHeight; y++)
        {
            for (var x = 0; x < AttributeWidth; x++)
            {
                result[ScreenRamRowOffsets[y] + x] = (byte)_bitmapColors[y * AttributeWidth + x];
            }
        }

        var row = 5;
        for (var y = 0; y < ScreenHeight; y++)
        {
            // Underlay sprites
            for (var x = 0; x < UnderlayColumns * SpriteBlockWidth; x++)
            {
                var srow = (row + (x % 3)) & 0x3f;
                int ofs;
                if (x >= 5 * 3 && y < 128)
                {
                    // Sprite 6
                    ofs = 0x5400 + ((((y / 2) & 0x7) ^ 0x7) * 0x40) + srow;
                }
                else
                {
                    // Sprites 1-5
                    ofs = UnderlayRowOffsets[y >> 1] + srow + x / 3 * 0x40;
                }
                result[ofs] = GetByte(_underlay, y * UnderlayColumns * SpriteWidth + (x << 3));
            }

            // Bug sprites
            for (var x = 0; x < BugBlockWidth; x++)
            {
                var srow = (row + (x % 3)) & 0x3f;
                result[BugHiresSpriteRowOffsets[y >> 1] + srow] = GetByte(_bug, y * BugSprites * SpriteWidth + (x << 3));
                result[BugMultiSpriteRowOffsets[y >> 1] + srow] = GetByte(_bug, (y * BugSprites + 1) * SpriteWidth + (x << 3));
            }
            if ((y & 1) == 0)
            {
                row += 3;
            }
            if (row > 0x3f)
            {
                row &= 0x3f;
            }
            else if (row == 0x3f)
            {
                row = 0;
            }
        }

        if (ntsc)
        {
            codeGeneration.AdjustCodeForNtsc();
        }
        else
        {
            var adjustments = codeGeneration.NtscAdjustments;
            var deltaMapping = NtscAdjustment.GetDeltaMapping(adjustments);
            foreach (var delta in deltaMapping)
            {
                result[0x20a0 + delta.Value] = (byte)delta.Key;
            }
            for (var i = 0; i < adjustments.Count; i++)
            {
                result[0xc00 + i] = adjustments[i].GetValue(deltaMapping);
            }
            var palRtsAddress = codeGeneration.Code.Count - 1 + 0x1000;
            var ntscRtsAddress = codeGeneration.NtscRtsOffset + 0x1000;
            result[0xcfb] = (byte)palRtsAddress;
            result[0xcfc] = (byte)(palRtsAddress >> 8);
            result[0xcfd] = (byte)ntscRtsAddress;
            result[0xcfe] = (byte)(ntscRtsAddress >> 8);
            result[0xcff] = (byte)(adjustments.Count - 1);
        }

        result.RemoveRange(0, 0x100);

        // Load address
        result.Insert(0, 0x00);
        result.Insert(1, 0x10);
        codeGeneration.PadCode();
        result.InsertRange(2, codeGeneration.Code);

        Profiler.EndSample();
        Profiler.EndSample();

        return new NuflixExportResult
        {
            Bytes = result.ToArray(),
            FreeCycles = codeGeneration.FreeCycles,
            Error = codeGeneration.Error,
        };
    }

    private (List<RegisterUpdate>[], List<RegisterUpdate>) FindRegisterUpdates()
    {
        var rowUpdates = new List<RegisterUpdate>[AttributeHeight];
        var deferredUpdates = new List<RegisterUpdate>();
        var prevRowColors = new int[UnderlayColumns];
        for (var i = 0; i < UnderlayColumns; i++)
        {
            prevRowColors[i] = _underlayColors[i] & 0xf;
        }
        for (var y = 0; y < AttributeHeight; y++)
        {
            var vsection = y & 3;
            var updates = new List<RegisterUpdate>();
            rowUpdates[y] = updates;
            var screenY = y << 1;
            var nextScreenY = screenY + 2;
            // Update underlay colours for the next two rows
            for (var row = 1; row <= 2; row++)
            {
                var uy = screenY + row;
                if (uy >= ScreenHeight)
                {
                    break;
                }
                for (var c = 0; c < UnderlayColumns; c++)
                {
                    var nextUnderlay = _underlayColors[uy * UnderlayColumns + c];
                    var color = nextUnderlay & 0xf;
                    if (color == prevRowColors[c])
                    {
                        continue;
                    }
                    prevRowColors[c] = color;
                    if (nextUnderlay < 0)
                    {
                        for (var uyLast = uy + 1; uyLast < ScreenHeight; uyLast++)
                        {
                            if (_underlayColors[uyLast * UnderlayColumns + c] >= 0)
                            {
                                deferredUpdates.Add(RegisterUpdate.UnderlayColorUpdate(c, color, uy, uyLast));
                                break;
                            }
                        }
                    }
                    else
                    {
                        updates.Add(RegisterUpdate.UnderlayColorUpdate(c, color, uy));
                    }
                }
            }
            if (y < AttributeHeight - 1)
            {
                // Update bug colours for the next row
                for (var s = 0; s < BugColorSlots; s++)
                {
                    var nextBug = _bugColors[(y + 1) * BugColorSlots + s];
                    var color = nextBug & 0xf;
                    if (color == (_bugColors[y * BugColorSlots + s] & 0xf))
                    {
                        continue;
                    }
                    if (nextBug < 0)
                    {
                        var lastScreenY = AttributeHeight;
                        for (var yLast = y + 2; yLast < AttributeHeight; yLast++)
                        {
                            if ((_bugColors[yLast * BugColorSlots + s] & 0xf) != color)
                            {
                                lastScreenY = yLast - 1;
                                break;
                            }
                        }
                        deferredUpdates.Add(RegisterUpdate.BugColorUpdate(s, color, nextScreenY, lastScreenY << 1));
                    }
                    else
                    {
                        updates.Add(RegisterUpdate.BugColorUpdate(s, color, nextScreenY));
                    }
                }
            }
            if (_borderColors[y + 1] != _borderColors[y])
            {
                updates.Add(RegisterUpdate.BorderColorUpdate(_borderColors[y + 1], nextScreenY));
            }
            updates.Add(RegisterUpdate.ScreenAddressUpdate(nextScreenY));
            updates.Add(RegisterUpdate.FliTriggerUpdate(nextScreenY));
            RegisterUpdate.SortUpdatesByTime(updates);
        }
        for (var i = 0; i < 8; i++)
        {
            deferredUpdates.Add(RegisterUpdate.SpriteYUpdate(i));
        }
        if (_image.BottomBackgroundColor != _image.TopBackgroundColor)
        {
            deferredUpdates.Add(RegisterUpdate.BackgroundColorUpdate(_image.BottomBackgroundColor));
        }
        deferredUpdates.Sort((c1, c2) =>
        {
            var cmpScreenY = c1.ScreenY.CompareTo(c2.ScreenY);
            if (cmpScreenY != 0)
            {
                return cmpScreenY;
            }
            var cmpLastScreenY = c1.LastScreenY.CompareTo(c2.LastScreenY);
            if (cmpLastScreenY != 0)
            {
                return cmpLastScreenY;
            }
            return c1.Address.CompareTo(c2.Address);
        }
        );
        return (rowUpdates, deferredUpdates);
    }

    public LayeredImage ImportNufli(byte[] bytes, Palette palette)
    {
        var result = new LayeredImage(palette);
        result.DirectlyEditing = true;
        var nufli = new Span<byte>(bytes)[2..];

        // Sprite colours
        var tableOffsets = new int[] { 0x400, 0x480, 0x800, 0x880, 0xc00, 0xc80 };

        result.BugColors[0] = nufli[0x1ff7]; // d027
        result.BugColors[1] = nufli[0x1ff1]; // d025
        result.BugColors[2] = nufli[0x1ff6]; // d02e
        result.BugColors[3] = nufli[0x1ff0]; // d026

        for (var c = 0; c < UnderlayColumns; c++)
        {
            result.UnderlayColors[c] = nufli[tableOffsets[c]];
        }

        for (var y = 0; y < AttributeHeight; y++)
        {
            var sy = (y << 1) + 1;
            if (y < AttributeHeight - 1)
            {
                for (var s = 0; s < BugColorSlots; s++)
                {
                    result.BugColors[(y + 1) * BugColorSlots + s] = result.BugColors[y * BugColorSlots + s];
                }
            }
            for (var c = 0; c < UnderlayColumns; c++)
            {
                var col = result.UnderlayColors[(sy - 1) * UnderlayColumns + c];
                var reg = nufli[tableOffsets[c] + y + 1];
                switch (reg >> 4)
                {
                    case 0x0:
                        col = reg & 0xf;
                        break;
                    case 0x5:
                        result.BugColors[(y + 1) * BugColorSlots + 1] = reg & 0xf;
                        break;
                    case 0x6:
                        result.BugColors[(y + 1) * BugColorSlots + 3] = reg & 0xf;
                        break;
                    case 0x7:
                        result.BugColors[(y + 1) * BugColorSlots + 0] = reg & 0xf;
                        break;
                    case 0xe:
                        result.BugColors[(y + 1) * BugColorSlots + 2] = reg & 0xf;
                        break;
                }
                result.UnderlayColors[sy * UnderlayColumns + c] = col;
                if (y < AttributeHeight - 1)
                {
                    result.UnderlayColors[(sy + 1) * UnderlayColumns + c] = col;
                }
            }
        }

        result.BugColors.CopyTo(result.ReferenceBugColors);

        // Bitmap
        for (var i = 0; i < 8000; i++)
        {
            var ofs = i % ScreenWidth;
            var y = ((i / ScreenWidth) << 3) + (ofs & 7);
            var x = ofs & ~7;
            SetByte(result.Bitmap, y * ScreenWidth + x, nufli[i < 0x1400 ? 0x4000 + i : i]);
        }

        // Attributes
        for (var y = 0; y < AttributeHeight; y++)
        {
            for (var x = 0; x < AttributeWidth; x++)
            {
                result.BitmapColors[y * AttributeWidth + x] = nufli[ScreenRamRowOffsets[y] + x];
            }
        }

        for (var y = 0; y < AttributeHeight; y++)
        {
            if ((y & 3) == 0)
            {
                continue;
            }
            result.BitmapColors[y * AttributeWidth] = 0xff;
            result.BitmapColors[y * AttributeWidth + 1] = 0xff;
            result.BitmapColors[y * AttributeWidth + 2] = 0xff;
        }

        // Moving sprite data where the NUFLIX tables expect them
        for (var i = 0; i < 0x100; i++)
        {
            nufli[0x400 + i] = nufli[i]; // Bottom bug sprite 7
            nufli[0x800 + i] = nufli[0x1200 + i]; // Bottom bug sprite 0
        }

        var row = 5;
        for (var y = 0; y < ScreenHeight; y++)
        {
            // Underlay sprites
            for (var x = 0; x < UnderlayColumns * SpriteBlockWidth; x++)
            {
                var srow = (row + (x % 3)) & 0x3f;
                int ofs;
                if (x >= 5 * 3 && y < 128)
                {
                    // Sprite 6
                    ofs = 0x5400 + ((((y / 2) & 0x7) ^ 0x7) * 0x40) + srow;
                }
                else
                {
                    // Sprites 1-5
                    ofs = UnderlayRowOffsets[y >> 1] + srow + x / 3 * 0x40;
                }
                SetByte(result.Underlay, y * UnderlayColumns * SpriteWidth + (x << 3), nufli[ofs]);
            }

            // Bug sprites
            for (var x = 0; x < BugBlockWidth; x++)
            {
                var srow = (row + (x % 3)) & 0x3f;
                SetByte(result.Bug, y * BugSprites * SpriteWidth + (x << 3), nufli[BugHiresSpriteRowOffsets[y >> 1] + srow]);
                SetByte(result.Bug, (y * BugSprites + 1) * SpriteWidth + (x << 3), nufli[BugMultiSpriteRowOffsets[y >> 1] + srow]);
            }
            if ((y & 1) == 0)
            {
                row += 3;
            }
            if (row > 0x3f)
            {
                row &= 0x3f;
            }
            else if (row == 0x3f)
            {
                row = 0;
            }
        }

        return result;
    }
}

public class NuflixExportResult
{
    public byte[] Bytes;
    public List<int> FreeCycles;
    public string Error;
}