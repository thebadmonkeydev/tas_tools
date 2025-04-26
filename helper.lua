-----------------------------------------------------------------------
-- Lua script for displaying "countdown" boxes to assist with
-- jumping on the required frame to achieve EarlyHammer
--
-- Created By: orangeexpo, August 9, 2018
-- For use with the TAS "orange-nodeath-eh-v0.5.fm2"
-----------------------------------------------------------------------


-- Allows you to adjust how many frames between each box being filled in
local countdown_delay = 45

local goodframe_2_1 = 17821 -- 688 lag, 289 ingame clock, 18212 end of level lag frame
local goodframe_2_2 = 19725 -- 773 lag, 285 ingame clock, 20207 end of level lag frame
local goodframe_2_f = 22672 -- 872 lag, 277 ingame clock, 23079 end of level lag frame

local screen_width  = 0x10 --256 pixels, 16 blocks
local screen_height = 0x0F --240 pixels, 15 blocks

-- Allows you to adjust how many boxes to display on screen
local nboxes = 9

-- Allows you to adjust how big the boxes are
local box_size = 20
local space_size = 5
local box_y = 20

local rtw = 0
local eol_frame = 0

local floorhalf = math.floor(nboxes/2)

local box_colors = {}

function init_box_colors()
    for i=1, nboxes, 1 do
        box_colors[i] = "#ffffff80"
    end
end

function display_boxes()
    local x_mid = ((screen_width*16)/2) + 8
    local box1_x = x_mid - (box_size*0.5) - ((box_size+space_size)*floorhalf)
    for i=0, nboxes-1, 1 do
        local x = box1_x+(i*(box_size+space_size))
        local b = {x, box_y, x + box_size, box_y + box_size, box_colors[i+1], "white"}
        gui.drawrect(unpack(b))
    end
end

function update_box_colors(frame, curr)
    -- Turn the middle box green on 'frame'
    -- First and last boxes are paired, and so on until the middle one
    -- Threshold for the first box is 'frame' - (floorhalf*countdown_delay)
    if curr < (frame - (floorhalf*countdown_delay)) or curr > (frame + 180) then
        init_box_colors()
        return false
    end
    for i=1, floorhalf+1, 1 do
        if curr >= (frame - ((floorhalf-(i-1))*countdown_delay)) then
            box_colors[i] = "green"
            box_colors[nboxes-(i-1)] = "green"
        end
    end
    return true
end

function doit()
    curr = emu.framecount()
    if update_box_colors(goodframe_2_1, curr) then
        -- do nothing
    elseif update_box_colors(goodframe_2_2, curr) then
        -- do nothing
    elseif update_box_colors(goodframe_2_f, curr) then
        -- do nothing
--    elseif update_box_colors(goodframe_ph, curr) then
--        -- do nothing
    end

    --display_boxes()
end

function toBits(num,bits)
    -- returns a table of bits, most significant first.
    bits = bits or math.max(1, select(2, math.frexp(num)))
    local t = {} -- will contain the bits        
    for b = bits, 1, -1 do
        t[b] = math.fmod(num, 2)
        num = math.floor((num - t[b]) / 2)
    end
    return t
end

function get_direction(least_bits)
    local human = ''
    if least_bits == 0 then
        human = "right"
    elseif least_bits == 1 then
        human = "left"
    elseif least_bits == 2 then
        human = "down"
    elseif least_bits == 3 then
        human = "up"
    end

    return human
end

function preframe_calculations()
    local trtw = memory.readbyte(0x0014) -- Return to world flag
    if trtw == 0 and rtw == 1 then
        eol_frame = emu.framecount() - 1
        -- record frame when set back to 0
        print("EOL Frame: ", eol_frame)
        -- Death position return values are reset in this frame
        -- can we use them to determine what level we just finished?
    end
    rtw = trtw

    -- EOL frame used: 3572
    if emu.framecount() == eol_frame + 29 then -- Determine facing (frame: 3600)
      local bits = toBits(memory.readbyte(0x0784), 8)
      local hleast = bits[7] 
      local lleast = bits[8]
      local least = (hleast * 2) + (lleast)
      print("Facing: ", table.concat(toBits(memory.readbyte(0x0784), 8)), " : ", get_direction(least))
    end
    if emu.framecount() == eol_frame + 29 + 39 then -- Determine move direction (frame: 3639)
      local bits = toBits(memory.readbyte(0x0784), 8)
      local hleast = bits[7] 
      local lleast = bits[8]
      local highest = bits[1]
      local least = (hleast * 2) + (lleast)
      if highest == 0 then
        print("Moving: ", table.concat(toBits(memory.readbyte(0x0784), 8)), " : ", get_direction((least - 1) % 4))
      elseif highest == 1 then
        print("Moving: ", table.concat(toBits(memory.readbyte(0x0784), 8)), " : ", get_direction((least + 1) % 4))
      end
    end
    doit()
end


init_box_colors()
gui.register(preframe_calculations)

