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
}

//==============================================================================
PurpleSpiceProcessor::PurpleSpiceProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    const juce::ScopedLock sl (stateLock);
    engine.reseed ((uint32_t) juce::Random::getSystemRandom().nextInt());
    engine.bpm = 140.0;
    engine.generate();
    pushHistory();
    publish();
}

void PurpleSpiceProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
    lastKit = -1;
    numActive = 0;
    running.store (false);
    playPos.store (-1.0);
    midiOut.ensureSize (8192);
}

bool PurpleSpiceProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* PurpleSpiceProcessor::createEditor()
{
    return new PurpleSpiceEditor (*this);
}

//==============================================================================
//  Audio-Thread
//==============================================================================
void PurpleSpiceProcessor::addTrigger (int sample, int type, float velocity, int pitch, float pan)
{
    if (numTriggers < maxTriggers)
        triggers[numTriggers++] = { sample, type, velocity, pitch, pan };
}

void PurpleSpiceProcessor::fire (const spice::NoteEvent& e, double offsetBeats, double blockBeats, int numSamples)
{
    auto toSample = [&] (double beats)
    {
        return juce::jlimit (0, numSamples - 1, (int) (beats / blockBeats * numSamples));
    };

    const bool open  = e.kind == spice::kOpen;
    const int note   = juce::jlimit (0, 127, rootNote.load() + e.pitch);
    const int ch     = open ? 2 : 1;
    const int sample = toSample (offsetBeats);

    // derselbe Ton klingt noch? Dann erst sauber beenden.
    for (int i = numActive; --i >= 0;)
    {
        if (active[i].note == note && active[i].channel == ch)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (ch, note), sample);
            active[i] = active[--numActive];
        }
    }

    midiOut.addEvent (juce::MidiMessage::noteOn (ch, note, (juce::uint8) juce::jlimit (1, 127, e.velocity)), sample);

    // eingebauter Sound: Rolls wandern leicht im Stereobild
    float pan = 0.0f;
    if (e.kind == spice::kRoll)        { rollFlip = ! rollFlip; pan = rollFlip ? 0.4f : -0.4f; }
    else if (e.kind == spice::kGhost)  { pan = 0.2f; }
    addTrigger (sample, open ? trigOpen : trigClosed, (float) e.velocity / 127.0f, e.pitch, pan);

    const double end = offsetBeats + e.length;
    if (end < blockBeats)
    {
        const int offSample = juce::jmax (sample, toSample (end));
        midiOut.addEvent (juce::MidiMessage::noteOff (ch, note), offSample);
        if (open) addTrigger (offSample, trigRelease, 0.0f, 0, 0.0f);
    }
    else if (numActive < maxActive)
    {
        active[numActive++] = { note, ch, open, end - blockBeats };
    }
    else
    {
        midiOut.addEvent (juce::MidiMessage::noteOff (ch, note), numSamples - 1);
    }
}

void PurpleSpiceProcessor::flushActive (int samplePosition)
{
    for (int i = 0; i < numActive; ++i)
        midiOut.addEvent (juce::MidiMessage::noteOff (active[i].channel, active[i].note), samplePosition);
    if (numActive > 0)
        addTrigger (samplePosition, trigRelease, 0.0f, 0, 0.0f);
    numActive = 0;
}

void PurpleSpiceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    midiOut.clear();
    numTriggers = 0;

    // neues Pattern vom Overlay uebernehmen (ohne zu blockieren)
    if (patternVersion.load() != seenVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (patternLock);
        if (lock.isLocked())
        {
            audioPattern = shared;
            seenVersion = patternVersion.load();
        }
    }

    // Host-Transport und Tempo
    double bpm = lastBpm.load(), ppq = 0.0;
    bool hostPlaying = false, havePpq = false;

    if (auto* head = getPlayHead())
    {
        if (const auto info = head->getPosition())
        {
            if (const auto b = info->getBpm())
            {
                if (*b >= 20.0 && *b <= 999.0)
                {
                    bpm = *b;
                    haveHostBpm.store (true);
                }
            }
            hostPlaying = info->getIsPlaying();
            if (const auto q = info->getPpqPosition())
            {
                ppq = *q;
                havePpq = true;
            }
        }
    }
    lastBpm.store (bpm);

    if (numSamples <= 0)
    {
        midi.clear();
        return;
    }

    const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const double blockBeats = numSamples * bpm / (60.0 * sr);

    auto toSample = [&] (double beats)
    {
        return juce::jlimit (0, numSamples - 1, (int) (beats / blockBeats * numSamples));
    };

    // 1) faellige Note-Offs
    for (int i = numActive; --i >= 0;)
    {
        if (active[i].remaining < blockBeats)
        {
            const int at = toSample (active[i].remaining);
            midiOut.addEvent (juce::MidiMessage::noteOff (active[i].channel, active[i].note), at);
            if (active[i].open) addTrigger (at, trigRelease, 0.0f, 0, 0.0f);
            active[i] = active[--numActive];
        }
        else
        {
            active[i].remaining -= blockBeats;
        }
    }

    // 2) Sequenzer
    const bool wantHost = hostSync.load() && hostPlaying && havePpq;
    const bool wantRun  = wantHost || internalPlay.load();
    const double total  = audioPattern.totalBeats;

    if (wantRun && total > 0.0 && audioPattern.numEvents > 0)
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

            for (int i = 0; i < audioPattern.numEvents; ++i)
            {
                const auto& e = audioPattern.events[i];
                if (e.start >= segEnd) break;
                if (e.start >= segStart)
                    fire (e, blockOffset + (e.start - segStart), blockBeats, numSamples);
            }

            blockOffset += segLen;
            remaining   -= segLen;
            segStart     = segEnd >= total - 1.0e-9 ? 0.0 : segEnd;
        }

        pos = segStart;
        playPos.store (pos);
    }
    else if (running.load())
    {
        running.store (false);
        flushActive (0);
        playPos.store (-1.0);
    }

    // 3) eingebaute Hi-Hats
    const int k = kit.load();
    if (k != lastKit)
    {
        synth.setKit (k);
        lastKit = k;
    }

    // Trigger nach Zeit sortieren (wenige Eintraege, fast sortiert)
    for (int i = 1; i < numTriggers; ++i)
    {
        const Trigger t = triggers[i];
        int j = i - 1;
        while (j >= 0 && triggers[j].sample > t.sample) { triggers[j + 1] = triggers[j]; --j; }
        triggers[j + 1] = t;
    }

    if (buffer.getNumChannels() > 0)
    {
        float* left  = buffer.getWritePointer (0);
        float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        const float gain = juce::jlimit (0.0f, 1.0f, level.load()) * 1.25f;
        int cursor = 0;

        for (int i = 0; i < numTriggers; ++i)
        {
            const Trigger& t = triggers[i];
            const int at = juce::jlimit (0, numSamples, t.sample);
            if (at > cursor)
            {
                synth.render (left + cursor, right != nullptr ? right + cursor : nullptr, at - cursor, gain);
                cursor = at;
            }

            if (t.type == trigRelease)  synth.releaseOpen();
            else                        synth.trigger (t.type == trigOpen, t.velocity, t.pitch, t.pan);
        }

        if (cursor < numSamples)
            synth.render (left + cursor, right != nullptr ? right + cursor : nullptr, numSamples - cursor, gain);
    }

    midi.swapWith (midiOut);
}

//==============================================================================
//  Overlay-Schnittstelle
//==============================================================================
void PurpleSpiceProcessor::publish()
{
    engine.build (scratch);
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        shared = scratch;
    }
    ++patternVersion;
    ++uiVersion;
}

void PurpleSpiceProcessor::pushHistory()
{
    if (historyPos + 1 < (int) history.size())
        history.resize ((size_t) (historyPos + 1));

    Snapshot snap;
    snap.bars = engine.bars;
    snap.seed = engine.humanSeed;
    history.push_back (snap);

    if (history.size() > 40)
        history.erase (history.begin());

    historyPos = (int) history.size() - 1;
}

spice::Settings PurpleSpiceProcessor::getSettings() const
{
    const juce::ScopedLock sl (stateLock);
    return engine.s;
}

PurpleSpiceProcessor::SettingsResult PurpleSpiceProcessor::updateSettings (const spice::Settings& in)
{
    const juce::ScopedLock sl (stateLock);

    const spice::Settings old = engine.s;
    spice::Settings s = in;
    s.style    = juce::jlimit (0, (int) spice::kNumStyles - 1, s.style);
    s.bars     = juce::jlimit (1, spice::kMaxBars, s.bars);
    s.grid     = juce::jlimit (0, (int) spice::kNumGrids - 1, s.grid);
    s.kit      = juce::jlimit (0, spice::kNumKits - 1, s.kit);
    s.rootNote = juce::jlimit (24, 96, s.rootNote);
    for (float* v : { &s.rolls, &s.spice, &s.open, &s.dynamics, &s.swing, &s.humanize, &s.level })
        *v = juce::jlimit (0.0f, 1.0f, *v);
    engine.s = s;

    hostSync.store (s.hostSync);
    kit.store (s.kit);
    rootNote.store (s.rootNote);
    level.store (s.level);

    auto differs = [] (float a, float b) { return std::abs (a - b) > 1.0e-6f; };
    auto result = SettingsResult::soundOnly;

    if (s.style != old.style)
        engine.applyStyleDefaults();

    if (s.bars != (int) engine.bars.size())
    {
        engine.resize (s.bars);
        pushHistory();
        result = SettingsResult::resized;
    }
    else if (s.style != old.style || s.grid != old.grid || differs (s.rolls, old.rolls) || differs (s.spice, old.spice)
             || differs (s.open, old.open) || differs (s.dynamics, old.dynamics) || s.pitch != old.pitch
             || s.triplets != old.triplets || s.gaps != old.gaps || s.flams != old.flams)
    {
        engine.rebuild();
        result = SettingsResult::rebuilt;
    }

    publish();
    return result;
}

PurpleSpiceProcessor::View PurpleSpiceProcessor::getView() const
{
    const juce::ScopedLock sl (stateLock);

    View v;
    v.events.assign (scratch.events, scratch.events + scratch.numEvents);
    for (const auto& b : engine.bars)
        v.locked.push_back (b.locked);
    v.stats = engine.stats();
    return v;
}

bool PurpleSpiceProcessor::syncTempo()
{
    if (! haveHostBpm.load())
        return false;

    const double b = spice::Engine::bucket (lastBpm.load());
    const juce::ScopedLock sl (stateLock);
    if (std::abs (b - engine.bpm) < 0.5)
        return false;

    engine.bpm = b;
    engine.rebuild();
    publish();
    return true;
}

void PurpleSpiceProcessor::generate()
{
    const juce::ScopedLock sl (stateLock);
    if (haveHostBpm.load())
        engine.bpm = spice::Engine::bucket (lastBpm.load());
    engine.generate();
    pushHistory();
    publish();
}

void PurpleSpiceProcessor::vary()
{
    const juce::ScopedLock sl (stateLock);
    engine.vary();
    pushHistory();
    publish();
}

void PurpleSpiceProcessor::rerollBar (int bar)
{
    const juce::ScopedLock sl (stateLock);
    engine.rerollBar (bar);
    pushHistory();
    publish();
}

bool PurpleSpiceProcessor::toggleLock (int bar)
{
    const juce::ScopedLock sl (stateLock);
    if (bar < 0 || bar >= (int) engine.bars.size())
        return false;

    auto& b = engine.bars[(size_t) bar];
    b.locked = ! b.locked;
    if (! b.locked)
    {
        engine.rebuild();       // wieder an die aktuellen Regler anpassen
        publish();
    }
    else
    {
        ++uiVersion;
    }
    return b.locked;
}

bool PurpleSpiceProcessor::canStepHistory (int direction) const
{
    const juce::ScopedLock sl (stateLock);
    const int target = historyPos + direction;
    return target >= 0 && target < (int) history.size();
}

bool PurpleSpiceProcessor::stepHistory (int direction)
{
    const juce::ScopedLock sl (stateLock);
    const int target = historyPos + direction;
    if (target < 0 || target >= (int) history.size())
        return false;

    historyPos = target;
    engine.bars      = history[(size_t) target].bars;
    engine.humanSeed = history[(size_t) target].seed;
    engine.s.bars    = (int) engine.bars.size();
    engine.rebuild();
    publish();
    return true;
}

juce::File PurpleSpiceProcessor::writeTempMidi (int which) const
{
    const juce::ScopedLock sl (stateLock);

    if (spice::countEvents (scratch, which) == 0)
        return {};

    const double bpm = lastBpm.load();
    auto fileName = juce::String (which == 1 ? "PurpleSpice_OpenHat_" : "PurpleSpice_HiHat_")
                  + juce::String (spice::styleName (engine.s.style)) + "_" + juce::String (juce::roundToInt (bpm)) + "bpm";
    fileName = juce::File::createLegalFileName (fileName.replaceCharacter (' ', '_').replaceCharacter ('/', '_')) + ".mid";

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PurpleSpice");
    dir.createDirectory();
    const auto file = dir.getChildFile (fileName);

    const auto bytes = spice::writeMidiFile (scratch, bpm, engine.s.rootNote, which);
    if (! file.replaceWithData (bytes.data(), bytes.size()))
        return {};

    return file;
}

//==============================================================================
//  Projekt speichern / laden
//==============================================================================
void PurpleSpiceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (stateLock);
    const auto& s = engine.s;

    juce::XmlElement xml ("PURPLESPICE");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("style", s.style);
    xml.setAttribute ("grid", s.grid);
    xml.setAttribute ("rolls", (double) s.rolls);
    xml.setAttribute ("spice", (double) s.spice);
    xml.setAttribute ("open", (double) s.open);
    xml.setAttribute ("dynamics", (double) s.dynamics);
    xml.setAttribute ("swing", (double) s.swing);
    xml.setAttribute ("humanize", (double) s.humanize);
    xml.setAttribute ("level", (double) s.level);
    xml.setAttribute ("pitch", s.pitch ? 1 : 0);
    xml.setAttribute ("triplets", s.triplets ? 1 : 0);
    xml.setAttribute ("gaps", s.gaps ? 1 : 0);
    xml.setAttribute ("flams", s.flams ? 1 : 0);
    xml.setAttribute ("kit", s.kit);
    xml.setAttribute ("rootNote", s.rootNote);
    xml.setAttribute ("hostSync", s.hostSync ? 1 : 0);
    xml.setAttribute ("bpm", engine.bpm);
    xml.setAttribute ("seed", (int) engine.humanSeed);

    for (const auto& b : engine.bars)
    {
        auto* e = xml.createNewChildElement ("BAR");
        e->setAttribute ("base", (int) b.base);
        e->setAttribute ("var", (int) b.var);
        e->setAttribute ("locked", b.locked ? 1 : 0);

        juce::String hits;
        for (const auto& h : b.hits)
            hits << juce::String (h.start, 5) << ":" << juce::String (h.length, 5) << ":" << h.velocity << ":" << h.pitch << ":" << h.kind << ";";
        e->setAttribute ("hits", hits);
    }

    copyXmlToBinary (xml, destData);
}

void PurpleSpiceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName ("PURPLESPICE"))
        return;

    const juce::ScopedLock sl (stateLock);

    spice::Settings s;
    auto unit = [&] (const char* name, float fallback) { return juce::jlimit (0.0f, 1.0f, (float) xml->getDoubleAttribute (name, fallback)); };
    s.style    = juce::jlimit (0, (int) spice::kNumStyles - 1, xml->getIntAttribute ("style", s.style));
    s.grid     = juce::jlimit (0, (int) spice::kNumGrids - 1, xml->getIntAttribute ("grid", s.grid));
    s.rolls    = unit ("rolls", s.rolls);
    s.spice    = unit ("spice", s.spice);
    s.open     = unit ("open", s.open);
    s.dynamics = unit ("dynamics", s.dynamics);
    s.swing    = unit ("swing", s.swing);
    s.humanize = unit ("humanize", s.humanize);
    s.level    = unit ("level", s.level);
    s.pitch    = xml->getIntAttribute ("pitch", 1) != 0;
    s.triplets = xml->getIntAttribute ("triplets", 1) != 0;
    s.gaps     = xml->getIntAttribute ("gaps", 1) != 0;
    s.flams    = xml->getIntAttribute ("flams", 1) != 0;
    s.kit      = juce::jlimit (0, spice::kNumKits - 1, xml->getIntAttribute ("kit", s.kit));
    s.rootNote = juce::jlimit (24, 96, xml->getIntAttribute ("rootNote", s.rootNote));
    s.hostSync = xml->getIntAttribute ("hostSync", 1) != 0;

    std::vector<spice::Bar> loaded;
    for (auto* e : xml->getChildWithTagNameIterator ("BAR"))
    {
        if ((int) loaded.size() >= spice::kMaxBars)
            break;

        spice::Bar b;
        b.base   = (uint32_t) e->getIntAttribute ("base", 1);
        b.var    = (uint32_t) e->getIntAttribute ("var", 1);
        b.locked = e->getIntAttribute ("locked", 0) != 0;

        for (const auto& token : juce::StringArray::fromTokens (e->getStringAttribute ("hits"), ";", ""))
        {
            const auto f = juce::StringArray::fromTokens (token, ":", "");
            if (f.size() != 5) continue;

            spice::Hit h;
            h.start    = juce::jlimit (0.0, 3.999, f[0].getDoubleValue());
            h.length   = juce::jlimit (0.01, 4.0, f[1].getDoubleValue());
            h.velocity = juce::jlimit (1, 127, f[2].getIntValue());
            h.pitch    = juce::jlimit (-24, 24, f[3].getIntValue());
            h.kind     = juce::jlimit (0, (int) spice::kFlam, f[4].getIntValue());
            b.hits.push_back (h);
        }
        loaded.push_back (b);
    }

    engine.s = s;
    engine.bpm = spice::Engine::bucket (xml->getDoubleAttribute ("bpm", 140.0));

    if (! loaded.empty())
    {
        engine.bars      = loaded;                  // genau so, wie gespeichert
        engine.s.bars    = (int) loaded.size();
        engine.humanSeed = (uint32_t) xml->getIntAttribute ("seed", 1);
    }
    else
    {
        engine.generate();
    }

    hostSync.store (engine.s.hostSync);
    kit.store (engine.s.kit);
    rootNote.store (engine.s.rootNote);
    level.store (engine.s.level);

    history.clear();
    historyPos = -1;
    pushHistory();
    publish();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PurpleSpiceProcessor();
}
