#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
double positiveMod (double x, double m)
{
    if (m <= 0.0) return 0.0;
    const double r = std::fmod (x, m);
    return r < 0.0 ? r + m : r;
}

// Weisse Tasten ab MIDI 60 (in FL Studio "C5") loesen Akkord 1..8 aus
int chordForKey (int note)
{
    static const int map[13] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6, 7 };
    return (note >= 60 && note <= 72) ? map[note - 60] : -1;
}
}

//==============================================================================
PurrgressionProcessor::PurrgressionProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    const juce::ScopedLock sl (stateLock);
    engine.reseed ((uint32_t) juce::Random::getSystemRandom().nextInt());
    engine.generate();
    pushHistory();
    publish();
}

void PurrgressionProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
    lastSound = -1;
    numActive = 0;
    running.store (false);
    midiOut.ensureSize (4096);
    for (auto& t : trigOn) t = false;
}

bool PurrgressionProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* PurrgressionProcessor::createEditor()
{
    return new PurrgressionEditor (*this);
}

//==============================================================================
//  Audio-Thread
//==============================================================================
void PurrgressionProcessor::startNote (int note, int velocity, double offsetBeats, double lengthBeats,
                                       double blockBeats, int numSamples)
{
    if (note < 0 || note > 127)
        return;

    auto toSample = [&] (double beats)
    {
        return juce::jlimit (0, numSamples - 1, (int) (beats / blockBeats * numSamples));
    };

    const int onSample = toSample (offsetBeats);

    // Ton klingt noch? Dann erst sauber beenden.
    for (int i = numActive; --i >= 0;)
    {
        if (active[i].note == note)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (1, note), onSample);
            active[i] = active[--numActive];
        }
    }

    midiOut.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, velocity)), onSample);

    const double end = offsetBeats + lengthBeats;
    if (end < blockBeats)
        midiOut.addEvent (juce::MidiMessage::noteOff (1, note), juce::jmax (onSample, toSample (end)));
    else if (numActive < maxActive)
        active[numActive++] = { note, end - blockBeats };
    else
        midiOut.addEvent (juce::MidiMessage::noteOff (1, note), numSamples - 1);
}

void PurrgressionProcessor::flushActive (int samplePosition)
{
    for (int i = 0; i < numActive; ++i)
        midiOut.addEvent (juce::MidiMessage::noteOff (1, active[i].note), samplePosition);
    numActive = 0;
}

void PurrgressionProcessor::releaseTrigger (int index, int samplePosition)
{
    if (index < 0 || index >= purr::kMaxChords || ! trigOn[index])
        return;

    for (int k = 0; k < trigSize[index]; ++k)
        midiOut.addEvent (juce::MidiMessage::noteOff (1, trigNotes[index][k]), samplePosition);
    trigOn[index] = false;
}

void PurrgressionProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    midiOut.clear();

    // neue Akkordfolge vom Overlay uebernehmen (ohne zu blockieren)
    if (progVersion.load() != seenVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (progLock);
        if (lock.isLocked())
        {
            audioProg = shared;
            seenVersion = progVersion.load();
        }
    }

    // Host-Transport
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false, havePpq = false;

    if (auto* head = getPlayHead())
    {
        if (const auto info = head->getPosition())
        {
            if (const auto b = info->getBpm())
                bpm = *b;
            hostPlaying = info->getIsPlaying();
            if (const auto q = info->getPpqPosition())
            {
                ppq = *q;
                havePpq = true;
            }
        }
    }

    if (! (bpm >= 20.0 && bpm <= 999.0))
        bpm = 120.0;
    lastBpm.store (bpm);

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const double blockBeats = numSamples * bpm / (60.0 * sr);

    if (numSamples <= 0)
    {
        midi.clear();
        return;
    }

    auto toSample = [&] (double beats)
    {
        return juce::jlimit (0, numSamples - 1, (int) (beats / blockBeats * numSamples));
    };

    // 1) faellige Note-Offs
    for (int i = numActive; --i >= 0;)
    {
        if (active[i].remaining < blockBeats)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (1, active[i].note), toSample (active[i].remaining));
            active[i] = active[--numActive];
        }
        else
        {
            active[i].remaining -= blockBeats;
        }
    }

    // 2) eingehendes MIDI: Tasten spielen ganze Akkorde
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, numSamples - 1, meta.samplePosition);

        if (msg.isNoteOn())
        {
            const int idx = chordForKey (msg.getNoteNumber());
            if (idx >= 0 && idx < audioProg.numChords)
            {
                releaseTrigger (idx, at);
                trigSize[idx] = audioProg.chordSize[idx];
                for (int k = 0; k < trigSize[idx]; ++k)
                {
                    trigNotes[idx][k] = audioProg.chordNotes[idx][k];
                    midiOut.addEvent (juce::MidiMessage::noteOn (1, trigNotes[idx][k], msg.getVelocity()), at);
                }
                trigOn[idx] = true;
                if (! running.load())
                    currentChord.store (idx);
            }
        }
        else if (msg.isNoteOff())
        {
            const int idx = chordForKey (msg.getNoteNumber());
            if (idx >= 0)
            {
                releaseTrigger (idx, at);
                if (! running.load() && currentChord.load() == idx)
                    currentChord.store (-1);
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (int i = 0; i < purr::kMaxChords; ++i)
                releaseTrigger (i, at);
        }
    }

    // 3) Anhoeren per Klick auf eine Akkordkarte
    const int aud = auditionRequest.exchange (-1);
    if (aud >= 0 && aud < audioProg.numChords)
        for (int k = 0; k < audioProg.chordSize[aud]; ++k)
            startNote (audioProg.chordNotes[aud][k], 92, 0.0, 2.0, blockBeats, numSamples);

    // 4) Sequenzer
    const bool wantHost = hostSync.load() && hostPlaying && havePpq;
    const bool wantRun  = wantHost || internalPlay.load();
    const double total  = audioProg.totalBeats;

    if (wantRun && total > 0.0 && audioProg.numEvents > 0)
    {
        bool resync = ! running.load() || (wantHost != hostMode);

        if (wantHost)
        {
            if (! resync && std::abs (ppq - expectedPpq) > 0.02)
                resync = true;                      // Host ist gesprungen (Loop, Klick in die Timeline)
            expectedPpq = ppq + blockBeats;
        }

        if (resync)
        {
            flushActive (0);
            pos = wantHost ? positiveMod (ppq, total) : 0.0;
        }

        running.store (true);
        hostMode = wantHost;

        if (pos >= total)
            pos = positiveMod (pos, total);

        double segStart = pos, remaining = blockBeats, blockOffset = 0.0;

        for (int guard = 0; guard < 8 && remaining > 1.0e-12; ++guard)
        {
            const double segLen = juce::jmin (remaining, total - segStart);
            const double segEnd = segStart + segLen;

            for (int i = 0; i < audioProg.numEvents; ++i)
            {
                const auto& e = audioProg.events[i];
                if (e.start >= segEnd) break;
                if (e.start >= segStart)
                    startNote (e.note, e.velocity, blockOffset + (e.start - segStart), e.length, blockBeats, numSamples);
            }

            blockOffset += segLen;
            remaining   -= segLen;
            segStart     = segEnd >= total - 1.0e-9 ? 0.0 : segEnd;
        }

        pos = segStart;

        const int chord = juce::jlimit (0, juce::jmax (0, audioProg.numChords - 1),
                                        (int) (pos / juce::jmax (0.25, audioProg.beatsPerChord)));
        currentChord.store (chord);
        beatPhase.store ((float) (pos - std::floor (pos)));
    }
    else if (running.load())
    {
        running.store (false);
        flushActive (0);
        currentChord.store (-1);
        beatPhase.store (0.0f);
    }

    // 5) Vorschau-Synth: wird von genau den Noten gespielt, die auch als MIDI rausgehen
    const int snd = sound.load();
    if (snd != lastSound)
    {
        synth.setSound (snd);
        lastSound = snd;
    }

    if (buffer.getNumChannels() > 0)
    {
        float* left  = buffer.getWritePointer (0);
        float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        int cursor = 0;

        for (const auto meta : midiOut)
        {
            const int at = juce::jlimit (0, numSamples, meta.samplePosition);
            if (at > cursor)
            {
                synth.render (left + cursor, right != nullptr ? right + cursor : nullptr, at - cursor);
                cursor = at;
            }

            const auto msg = meta.getMessage();
            if (msg.isNoteOn())        synth.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())  synth.noteOff (msg.getNoteNumber());
        }

        if (cursor < numSamples)
            synth.render (left + cursor, right != nullptr ? right + cursor : nullptr, numSamples - cursor);
    }

    midi.swapWith (midiOut);
}

//==============================================================================
//  Overlay-Schnittstelle
//==============================================================================
void PurrgressionProcessor::publish()
{
    engine.buildProgData (scratch);
    {
        const juce::SpinLock::ScopedLockType lock (progLock);
        shared = scratch;
    }
    ++progVersion;
    ++uiVersion;
}

void PurrgressionProcessor::pushHistory()
{
    if (historyPos + 1 < (int) history.size())
        history.resize ((size_t) (historyPos + 1));

    Snapshot snap;
    snap.chords = engine.chords;
    snap.seed   = engine.humanSeed;
    snap.key    = engine.s.key;
    history.push_back (snap);

    if (history.size() > 48)
        history.erase (history.begin());

    historyPos = (int) history.size() - 1;
}

purr::Settings PurrgressionProcessor::getSettings() const
{
    const juce::ScopedLock sl (stateLock);
    return engine.s;
}

PurrgressionProcessor::SettingsResult PurrgressionProcessor::updateSettings (const purr::Settings& in)
{
    const juce::ScopedLock sl (stateLock);

    const purr::Settings old = engine.s;
    purr::Settings s = in;
    s.key       = juce::jlimit (0, 11, s.key);
    s.scale     = juce::jlimit (0, (int) purr::kNumScales - 1, s.scale);
    s.style     = juce::jlimit (0, (int) purr::kNumStyles - 1, s.style);
    s.numChords = juce::jlimit (2, purr::kMaxChords, s.numChords);
    s.duration  = juce::jlimit (0, 2, s.duration);
    s.voicing   = juce::jlimit (0, (int) purr::kNumVoicings - 1, s.voicing);
    s.pattern   = juce::jlimit (0, (int) purr::kNumPatterns - 1, s.pattern);
    s.octave    = juce::jlimit (-1, 1, s.octave);
    s.sound     = juce::jlimit (0, 3, s.sound);
    engine.s = s;

    hostSync.store (s.hostSync);
    sound.store (s.sound);

    auto result = SettingsResult::soundOnly;

    if (s.scale != old.scale || s.style != old.style)
    {
        if (s.key != old.key)
            engine.transpose (s.key - old.key);
        engine.generate();
        pushHistory();
        result = SettingsResult::regenerated;
    }
    else
    {
        const bool voiced = s.duration != old.duration || s.voicing != old.voicing || s.pattern != old.pattern
                         || s.octave != old.octave || s.bass != old.bass || s.voiceLead != old.voiceLead
                         || std::abs (s.humanize - old.humanize) > 1.0e-6f || std::abs (s.swing - old.swing) > 1.0e-6f;
        if (voiced)
            result = SettingsResult::revoiced;

        if (s.key != old.key)
        {
            engine.transpose (s.key - old.key);
            result = SettingsResult::transposed;
        }

        if (s.numChords != (int) engine.chords.size())
        {
            engine.resize (s.numChords);
            pushHistory();
            result = SettingsResult::resized;
        }
    }

    publish();
    return result;
}

std::vector<PurrgressionProcessor::ChordView> PurrgressionProcessor::getChordViews() const
{
    const juce::ScopedLock sl (stateLock);

    std::vector<ChordView> views;
    const int n = juce::jmin ((int) engine.chords.size(), scratch.numChords);

    for (int i = 0; i < n; ++i)
    {
        const auto& c = engine.chords[(size_t) i];
        ChordView v;
        v.name    = juce::String::fromUTF8 (engine.chordName (c).c_str());
        v.roman   = juce::String::fromUTF8 (engine.roman (c).c_str());
        v.notes   = juce::String::fromUTF8 (engine.chordNoteNames (c).c_str());
        v.locked  = c.locked;
        v.quality = purr::qualityOf (c.type);
        v.midi.assign (scratch.chordNotes[i], scratch.chordNotes[i] + scratch.chordSize[i]);
        views.push_back (std::move (v));
    }
    return views;
}

juce::String PurrgressionProcessor::getKeyLabel() const
{
    const juce::ScopedLock sl (stateLock);
    return juce::String::fromUTF8 (engine.noteName (engine.s.key).c_str()) + " "
         + juce::String::fromUTF8 (purr::scaleDef (engine.s.scale).name);
}

void PurrgressionProcessor::generate()
{
    const juce::ScopedLock sl (stateLock);
    engine.generate();
    pushHistory();
    publish();
}

void PurrgressionProcessor::vary()
{
    const juce::ScopedLock sl (stateLock);
    engine.vary();
    pushHistory();
    publish();
}

void PurrgressionProcessor::rerollChord (int index)
{
    const juce::ScopedLock sl (stateLock);
    engine.rerollChord (index);
    pushHistory();
    publish();
}

bool PurrgressionProcessor::toggleLock (int index)
{
    const juce::ScopedLock sl (stateLock);
    if (index < 0 || index >= (int) engine.chords.size())
        return false;

    auto& c = engine.chords[(size_t) index];
    c.locked = ! c.locked;
    if (historyPos >= 0 && historyPos < (int) history.size() && index < (int) history[(size_t) historyPos].chords.size())
        history[(size_t) historyPos].chords[(size_t) index].locked = c.locked;

    ++uiVersion;
    return c.locked;
}

bool PurrgressionProcessor::canStepHistory (int direction) const
{
    const juce::ScopedLock sl (stateLock);
    const int target = historyPos + direction;
    return target >= 0 && target < (int) history.size();
}

bool PurrgressionProcessor::stepHistory (int direction)
{
    const juce::ScopedLock sl (stateLock);
    const int target = historyPos + direction;
    if (target < 0 || target >= (int) history.size())
        return false;

    historyPos = target;
    const auto& snap = history[(size_t) target];
    engine.chords    = snap.chords;
    engine.humanSeed = snap.seed;
    engine.transpose (engine.s.key - snap.key);
    engine.s.numChords = (int) engine.chords.size();
    publish();
    return true;
}

juce::File PurrgressionProcessor::writeTempMidi() const
{
    const juce::ScopedLock sl (stateLock);

    juce::StringArray names;
    for (const auto& c : engine.chords)
        names.add (juce::String (engine.chordName (c)));

    auto fileName = "Purr_" + names.joinIntoString ("-");
    fileName = fileName.replaceCharacter ('#', 's').replaceCharacter ('/', '_').removeCharacters ("()");
    fileName = juce::File::createLegalFileName (fileName).substring (0, 80) + ".mid";

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Purrgression");
    dir.createDirectory();
    const auto file = dir.getChildFile (fileName);

    const auto bytes = purr::writeMidiFile (scratch, lastBpm.load());
    if (! file.replaceWithData (bytes.data(), bytes.size()))
        return {};

    return file;
}

//==============================================================================
//  Projekt speichern / laden
//==============================================================================
void PurrgressionProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (stateLock);
    const auto& s = engine.s;

    juce::XmlElement xml ("PURRGRESSION");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("key", s.key);
    xml.setAttribute ("scale", s.scale);
    xml.setAttribute ("style", s.style);
    xml.setAttribute ("duration", s.duration);
    xml.setAttribute ("colour", (double) s.colour);
    xml.setAttribute ("spice", (double) s.spice);
    xml.setAttribute ("humanize", (double) s.humanize);
    xml.setAttribute ("swing", (double) s.swing);
    xml.setAttribute ("voicing", s.voicing);
    xml.setAttribute ("pattern", s.pattern);
    xml.setAttribute ("octave", s.octave);
    xml.setAttribute ("bass", s.bass ? 1 : 0);
    xml.setAttribute ("voiceLead", s.voiceLead ? 1 : 0);
    xml.setAttribute ("sound", s.sound);
    xml.setAttribute ("hostSync", s.hostSync ? 1 : 0);
    xml.setAttribute ("seed", (int) engine.humanSeed);

    for (const auto& c : engine.chords)
    {
        auto* e = xml.createNewChildElement ("CHORD");
        e->setAttribute ("root", c.root);
        e->setAttribute ("type", c.type);
        e->setAttribute ("degree", c.degree);
        e->setAttribute ("level", c.level);
        e->setAttribute ("dia", c.dia ? 1 : 0);
        e->setAttribute ("locked", c.locked ? 1 : 0);
        e->setAttribute ("secOf", c.secOf);
        e->setAttribute ("secMinor", c.secMinor ? 1 : 0);
    }

    copyXmlToBinary (xml, destData);
}

void PurrgressionProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName ("PURRGRESSION"))
        return;

    const juce::ScopedLock sl (stateLock);

    purr::Settings s;
    s.key       = juce::jlimit (0, 11, xml->getIntAttribute ("key", s.key));
    s.scale     = juce::jlimit (0, (int) purr::kNumScales - 1, xml->getIntAttribute ("scale", s.scale));
    s.style     = juce::jlimit (0, (int) purr::kNumStyles - 1, xml->getIntAttribute ("style", s.style));
    s.duration  = juce::jlimit (0, 2, xml->getIntAttribute ("duration", s.duration));
    s.colour    = juce::jlimit (0.0f, 1.0f, (float) xml->getDoubleAttribute ("colour", s.colour));
    s.spice     = juce::jlimit (0.0f, 1.0f, (float) xml->getDoubleAttribute ("spice", s.spice));
    s.humanize  = juce::jlimit (0.0f, 1.0f, (float) xml->getDoubleAttribute ("humanize", s.humanize));
    s.swing     = juce::jlimit (0.0f, 1.0f, (float) xml->getDoubleAttribute ("swing", s.swing));
    s.voicing   = juce::jlimit (0, (int) purr::kNumVoicings - 1, xml->getIntAttribute ("voicing", s.voicing));
    s.pattern   = juce::jlimit (0, (int) purr::kNumPatterns - 1, xml->getIntAttribute ("pattern", s.pattern));
    s.octave    = juce::jlimit (-1, 1, xml->getIntAttribute ("octave", s.octave));
    s.bass      = xml->getIntAttribute ("bass", 1) != 0;
    s.voiceLead = xml->getIntAttribute ("voiceLead", 1) != 0;
    s.sound     = juce::jlimit (0, 3, xml->getIntAttribute ("sound", s.sound));
    s.hostSync  = xml->getIntAttribute ("hostSync", 1) != 0;

    std::vector<purr::Chord> loaded;
    for (auto* e : xml->getChildWithTagNameIterator ("CHORD"))
    {
        if ((int) loaded.size() >= purr::kMaxChords)
            break;

        purr::Chord c;
        c.root     = juce::jlimit (0, 11, e->getIntAttribute ("root", 0));
        c.type     = juce::jlimit (0, (int) purr::kNumTypes - 1, e->getIntAttribute ("type", 0));
        c.degree   = juce::jlimit (0, 7, e->getIntAttribute ("degree", 0));
        c.level    = juce::jlimit (0, 3, e->getIntAttribute ("level", 0));
        c.dia      = e->getIntAttribute ("dia", 0) != 0;
        c.locked   = e->getIntAttribute ("locked", 0) != 0;
        c.secOf    = juce::jlimit (-1, 11, e->getIntAttribute ("secOf", -1));
        c.secMinor = e->getIntAttribute ("secMinor", 0) != 0;
        loaded.push_back (c);
    }

    engine.s = s;
    if (loaded.size() >= 2)
    {
        engine.chords      = loaded;
        engine.s.numChords = (int) loaded.size();
        engine.humanSeed   = (uint32_t) xml->getIntAttribute ("seed", 1);
    }
    else
    {
        engine.generate();
    }

    hostSync.store (engine.s.hostSync);
    sound.store (engine.s.sound);

    history.clear();
    historyPos = -1;
    pushHistory();
    publish();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurrgressionProcessor();
}
