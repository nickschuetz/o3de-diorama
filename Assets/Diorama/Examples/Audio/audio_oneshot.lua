-- Audio one-shots from input: press the bound actions to hear the gem's bundled
-- sounds through DioramaAudioRequestBus (MiniAudio underneath). Put this on an
-- entity that also has a 2D Input Actions component with Button actions named
-- "blip", "hit", and "jump" (the audio_demo.py builder wires 1/2/3).

local AudioOneShot = {
    Properties = {
        Volume = { default = 0.8, description = "One-shot volume (0..1)." },
    },
}

function AudioOneShot:OnActivate()
    self.volume = self.Properties.Volume or 0.8
    self.tickHandler = TickBus.Connect(self)
end

function AudioOneShot:OnDeactivate()
    if self.tickHandler ~= nil then
        self.tickHandler:Disconnect()
        self.tickHandler = nil
    end
end

function AudioOneShot:OnTick(deltaTime, timePoint)
    local id = self.entityId
    local input = DioramaInputRequestBus.Event
    if input.WasPressedThisFrame(id, "blip") then
        DioramaAudioRequestBus.Broadcast.PlayOneShot("diorama/audio/blip.wav", self.volume)
    end
    if input.WasPressedThisFrame(id, "hit") then
        DioramaAudioRequestBus.Broadcast.PlayOneShot("diorama/audio/hit.wav", self.volume)
    end
    if input.WasPressedThisFrame(id, "jump") then
        DioramaAudioRequestBus.Broadcast.PlayOneShot("diorama/audio/jump.wav", self.volume)
    end
end

return AudioOneShot
