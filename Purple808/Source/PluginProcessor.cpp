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
Purple808Processor::Purple808Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    const juce::ScopedLock sl (stateLock);
    engine.reseed ((uint32_t) juce::Random::getSystemRandom().nextInt());
    engine.bpm = 140.0;
    engine.generate();
    pushHistory();
    pushSoundParams();
    publish();
}

void Purple808Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    reverb.setSampleRate (sampleRate);
    reverb.reset();
    lastReverb = -1.0f;
    hpState = 0.0f;
    numActive = 0;
    running = false;
    playPos.store (-1.0);
    midiOut.ensureSize (4096);

    const auto size = (size_t) juce::jmax (16384, samplesPerBlock);
    monoBuf.assign (size, 0.0f);
    wetL.assign (size, 0.0f);
    wetR.assign (size, 0.0f);
}

bool Purple808Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* Purple808Processor::createEditor()
{
    return new Purple808Editor (*this);
}

//==============================================================================
//  Audio-Thread
//==============================================================================
void Purple808Processor::addTrigger (int sample, int type, int note, float velocity, int serial)
{
    if (numTriggers < maxTriggers)
        triggers[numTriggers++] = { sample, type, note, velocity, serial };
}

void Purple808Processor::fire (const p808::NoteEvent& e, double offsetBeats, double blockBeats, int numSamples)
{
    auto toSample = [&] (double beats)
    {
        return juce::jlimit (0, numSamples - 1, (int) (beats / blockBeats * numSamples));
    };

    const int note   = juce::jlimit (0, 127, e.note);
    const int sample = toSample (offsetBeats);
    const int serial = ++serialCounter;

    // dieselbe Tonhoehe klingt noch? Dann erst sauber beenden.
    for (int i = numActive; --i >= 0;)
    {
        if (active[i].note == note)
        {
            midiOut.addEvent (juce::MidiMessage::noteOff (1, note), sample);
            active[i] = active[--numActive];
        }
    }

    midiOut.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, e.velocity)), sample);
    addTrigger (sample, e.kind == p808::kSlide ? trigGlide : trigNote, note, (float) e.velocity / 127.0f, serial);

    const double end = offsetBeats + e.length;
    if (end < blockBeats)
    {
        const int offSample = juce::jmax (sample, toSample (end));
        midiOut.addEvent (juce::MidiMessage::noteOff (1, note), offSample);
        addTrigger (offSample, trigOff, note, 0.0f, serial);
    }
    else if (numActive < maxActive)
    {
        active[numActive++] = { note, serial, end - blockBeats };
    }
    else
    {
        midiOut.addEvent (juce::MidiMessage::noteOff (1, note), numSamples - 1);
        addTrigger (numSamples - 1, trigOff, note, 0.0f, serial);
    }
}

void Purple808Processor::flushActive (int samplePosition)
{
    for (int i = 0; i < numActive; ++i)
    {
        midiOut.addEvent (juce::MidiMessage::noteOff (1, active[i].note), samplePosition);
        addTrigger (samplePosition, trigOff, active[i].note, 0.0f, active[i].serial);
    }
    numActive = 0;
}

void Purple808Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    midiOut.clear();
    numTriggers = 0;

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

    if ((size_t) numSamples > monoBuf.size())      // sollte nie vorkommen, aber sicher ist sicher
    {
        monoBuf.assign ((size_t) numSamples, 0.0f);
        wetL.assign ((size_t) numSamples, 0.0f);
        wetR.assign ((size_t) numSamples, 0.0f);
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
            midiOut.addEvent (juce::MidiMessage::noteOff (1, active[i].note), at);
            addTrigger (at, trigOff, active[i].note, 0.0f, active[i].serial);
            active[i] = active[--numActive];
        }
        else
        {
            active[i].remaining -= blockBeats;
        }
    }

    // 2) Anspielen aus dem Overlay (808 ausgewaehlt)
    const int aud = auditionNote.exchange (-1);
    if (aud >= 0)
    {
        p808::NoteEvent e;
        e.note = aud; e.velocity = 112; e.length = 1.5; e.kind = p808::kNote;
        fire (e, 0.0, blockBeats, numSamples);
    }

    // 3) Sequenzer
    const bool wantHost = hostSync.load() && hostPlaying && havePpq;
    const bool wantRun  = wantHost || internalPlay.load();
    const double total  = audioPattern.totalBeats;

    if (wantRun && total > 0.0 && audioPattern.numEvents > 0)
    {
        bool resync = ! running || (wantHost != hostMode);

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

        running = true;
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
    else if (running)
    {
        running = false;
        flushActive (0);
        playPos.store (-1.0);
    }

    // 4) 808-Synth
    p808::SoundParams sp;
    sp.kit = kit.load(); sp.distortion = pDistortion.load(); sp.tone = pTone.load(); sp.punch = pPunch.load();
    sp.decay = pDecay.load(); sp.glide = pGlide.load(); sp.sub = pSub.load(); sp.level = pLevel.load();
    synth.setParams (sp);

    // Trigger nach Zeit sortieren; bei gleicher Zeit zuerst Note-Offs
    for (int i = 1; i < numTriggers; ++i)
    {
        const Trigger t = triggers[i];
        int j = i - 1;
        while (j >= 0 && (triggers[j].sample > t.sample
                          || (triggers[j].sample == t.sample && triggers[j].type != trigOff && t.type == trigOff)))
        {
            triggers[j + 1] = triggers[j];
            --j;
        }
        triggers[j + 1] = t;
    }

    float* mono = monoBuf.data();
    std::fill (mono, mono + numSamples, 0.0f);
    int cursor = 0;

    for (int i = 0; i < numTriggers; ++i)
    {
        const Trigger& t = triggers[i];
        const int at = juce::jlimit (0, numSamples, t.sample);
        if (at > cursor)
        {
            synth.render (mono + cursor, at - cursor);
            cursor = at;
        }

        if (t.type == trigNote)        synth.noteOn (t.note, t.velocity, t.serial);
        else if (t.type == trigGlide)  synth.glideTo (t.note, t.serial);
        else                           synth.noteOff (t.serial);
    }
    if (cursor < numSamples)
        synth.render (mono + cursor, numSamples - cursor);

    // 5) Hall nur auf den Anteil oberhalb von ca. 180 Hz, damit der Sub trocken und fest bleibt
    const float rv = juce::jlimit (0.0f, 1.0f, pReverb.load());
    const bool useReverb = rv > 0.004f;
    if (useReverb)
    {
        if (std::abs (rv - lastReverb) > 0.001f)
        {
            juce::Reverb::Parameters rp;
            rp.roomSize   = 0.45f + 0.45f * rv;
            rp.damping    = 0.5f;
            rp.wetLevel   = 0.33f;
            rp.dryLevel   = 0.0f;
            rp.width      = 1.0f;
            rp.freezeMode = 0.0f;
            reverb.setParameters (rp);
            lastReverb = rv;
        }

        const float hpCoef = (float) std::exp (-6.283185307179586 * 180.0 / sr);
        for (int i = 0; i < numSamples; ++i)
        {
            hpState = hpCoef * hpState + (1.0f - hpCoef) * mono[i];
            wetL[(size_t) i] = wetR[(size_t) i] = mono[i] - hpState;
        }
        reverb.processStereo (wetL.data(), wetR.data(), numSamples);
    }

    const float wetGain = 1.6f * rv;
    for (int ch = 0; ch < buffer.getNumChannels() && ch < 2; ++ch)
    {
        float* out = buffer.getWritePointer (ch);
        const float* wet = ch == 0 ? wetL.data() : wetR.data();
        for (int i = 0; i < numSamples; ++i)
            out[i] = juce::jlimit (-1.0f, 1.0f, mono[i] + (useReverb ? wet[i] * wetGain : 0.0f));
    }

    midi.swapWith (midiOut);
}

//==============================================================================
//  Overlay-Schnittstelle
//==============================================================================
void Purple808Processor::publish()
{
    engine.build (scratch);
    {
        const juce::SpinLock::ScopedLockType lock (patternLock);
        shared = scratch;
    }
    ++patternVersion;
    ++uiVersion;
}

void Purple808Processor::pushSoundParams()
{
    const auto& s = engine.s;
    hostSync.store (s.hostSync);
    kit.store (s.kit);
    pDistortion.store (s.distortion);
    pTone.store (s.tone);
    pPunch.store (s.punch);
    pDecay.store (s.decay);
    pGlide.store (s.glide);
    pSub.store (s.sub);
    pReverb.store (s.reverb);
    pLevel.store (s.level);
}

void Purple808Processor::pushHistory()
{
    if (historyPos + 1 < (int) history.size())
        history.resize ((size_t) (historyPos + 1));

    Snapshot snap;
    snap.bars = engine.bars;
    snap.prog = engine.progSeed;
    snap.seed = engine.humanSeed;
    history.push_back (snap);

    if (history.size() > 40)
        history.erase (history.begin());

    historyPos = (int) history.size() - 1;
}

p808::Settings Purple808Processor::getSettings() const
{
    const juce::ScopedLock sl (stateLock);
    return engine.s;
}

void Purple808Processor::updateSettings (const p808::Settings& in)
{
    const juce::ScopedLock sl (stateLock);

    const p808::Settings old = engine.s;
    p808::Settings s = in;
    s.style  = juce::jlimit (0, (int) p808::kNumStyles - 1, s.style);
    s.bars   = juce::jlimit (1, p808::kMaxBars, s.bars);
    s.key    = juce::jlimit (0, 11, s.key);
    s.scale  = juce::jlimit (0, (int) p808::kNumScales - 1, s.scale);
    s.octave = juce::jlimit (-1, 1, s.octave);
    s.kit    = juce::jlimit (0, p808::kNumKits - 1, s.kit);
    for (float* v : { &s.density, &s.slides, &s.spice, &s.movement, &s.gate, &s.swing,
                      &s.distortion, &s.tone, &s.punch, &s.decay, &s.glide, &s.sub, &s.reverb, &s.level })
        *v = juce::jlimit (0.0f, 1.0f, *v);
    engine.s = s;

    auto differs = [] (float a, float b) { return std::abs (a - b) > 1.0e-6f; };

    if (s.style != old.style)
        engine.applyStyleDefaults();

    if (s.bars != (int) engine.bars.size())
    {
        engine.resize (s.bars);
        pushHistory();
    }
    else if (s.style != old.style || s.scale != old.scale || differs (s.density, old.density) || differs (s.slides, old.slides)
             || differs (s.spice, old.spice) || differs (s.movement, old.movement) || s.stutter != old.stutter
             || s.octaves != old.octaves || s.triplets != old.triplets || s.gaps != old.gaps)
    {
        engine.rebuild();
    }

    pushSoundParams();
    publish();
}

Purple808Processor::View Purple808Processor::getView() const
{
    const juce::ScopedLock sl (stateLock);

    View v;
    v.events.assign (scratch.events, scratch.events + scratch.numEvents);
    const int root = engine.rootMidi();
    for (const auto& b : engine.bars)
    {
        v.locked.push_back (b.locked);
        v.barLabels.add (juce::String (p808::noteName (root + b.root)));
    }
    v.stats = engine.stats();
    v.rootPitchClass = ((root % 12) + 12) % 12;
    return v;
}

bool Purple808Processor::syncTempo()
{
    if (! haveHostBpm.load())
        return false;

    const double b = p808::Engine::bucket (lastBpm.load());
    const juce::ScopedLock sl (stateLock);
    if (std::abs (b - engine.bpm) < 0.5)
        return false;

    engine.bpm = b;
    engine.rebuild();
    publish();
    return true;
}

void Purple808Processor::audition()
{
    const juce::ScopedLock sl (stateLock);
    auditionNote.store (engine.rootMidi());
}

void Purple808Processor::generate()
{
    const juce::ScopedLock sl (stateLock);
    if (haveHostBpm.load())
        engine.bpm = p808::Engine::bucket (lastBpm.load());
    engine.generate();
    pushHistory();
    publish();
}

void Purple808Processor::vary()
{
    const juce::ScopedLock sl (stateLock);
    engine.vary();
    pushHistory();
    publish();
}

void Purple808Processor::rerollBar (int bar)
{
    const juce::ScopedLock sl (stateLock);
    engine.rerollBar (bar);
    pushHistory();
    publish();
}

bool Purple808Processor::toggleLock (int bar)
{
    const juce::ScopedLock sl (stateLock);
    if (bar < 0 || bar >= (int) engine.bars.size())
        return false;

    auto& b = engine.bars[(size_t) bar];
    b.locked = ! b.locked;
    if (! b.locked)
    {
        engine.rebuild();
        publish();
    }
    else
    {
        ++uiVersion;
    }
    return b.locked;
}

bool Purple808Processor::canStepHistory (int direction) const
{
    const juce::ScopedLock sl (stateLock);
    const int target = historyPos + direction;
    return target >= 0 && target < (int) history.size();
}

bool Purple808Processor::stepHistory (int direction)
{
    const juce::ScopedLock sl (stateLock);
    const int target = historyPos + direction;
    if (target < 0 || target >= (int) history.size())
        return false;

    historyPos = target;
    engine.bars      = history[(size_t) target].bars;
    engine.progSeed  = history[(size_t) target].prog;
    engine.humanSeed = history[(size_t) target].seed;
    engine.s.bars    = (int) engine.bars.size();
    engine.rebuild();
    publish();
    return true;
}

juce::File Purple808Processor::writeTempMidi() const
{
    const juce::ScopedLock sl (stateLock);
    if (scratch.numEvents == 0)
        return {};

    const double bpm = lastBpm.load();
    auto fileName = juce::String ("Purple808_") + juce::String (p808::styleName (engine.s.style)) + "_"
                  + juce::String (p808::noteName (engine.rootMidi())) + "_" + juce::String (juce::roundToInt (bpm)) + "bpm";
    fileName = fileName.replaceCharacter (' ', '_').replaceCharacter ('/', '_').replaceCharacter ('#', 's');
    fileName = juce::File::createLegalFileName (fileName) + ".mid";

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Purple808");
    dir.createDirectory();
    const auto file = dir.getChildFile (fileName);

    const auto bytes = p808::writeMidiFile (scratch, bpm);
    if (! file.replaceWithData (bytes.data(), bytes.size()))
        return {};

    return file;
}

//==============================================================================
//  Projekt speichern / laden
//==============================================================================
void Purple808Processor::getStateInformation (juce::MemoryBlock& destData)
{
    const juce::ScopedLock sl (stateLock);
    const auto& s = engine.s;

    juce::XmlElement xml ("PURPLE808");
    xml.setAttribute ("version", 1);
    xml.setAttribute ("style", s.style);
    xml.setAttribute ("key", s.key);
    xml.setAttribute ("scale", s.scale);
    xml.setAttribute ("octave", s.octave);
    xml.setAttribute ("density", (double) s.density);
    xml.setAttribute ("slides", (double) s.slides);
    xml.setAttribute ("spice", (double) s.spice);
    xml.setAttribute ("movement", (double) s.movement);
    xml.setAttribute ("gate", (double) s.gate);
    xml.setAttribute ("swing", (double) s.swing);
    xml.setAttribute ("stutter", s.stutter ? 1 : 0);
    xml.setAttribute ("octaves", s.octaves ? 1 : 0);
    xml.setAttribute ("triplets", s.triplets ? 1 : 0);
    xml.setAttribute ("gaps", s.gaps ? 1 : 0);
    xml.setAttribute ("hostSync", s.hostSync ? 1 : 0);
    xml.setAttribute ("kit", s.kit);
    xml.setAttribute ("distortion", (double) s.distortion);
    xml.setAttribute ("tone", (double) s.tone);
    xml.setAttribute ("punch", (double) s.punch);
    xml.setAttribute ("decay", (double) s.decay);
    xml.setAttribute ("glide", (double) s.glide);
    xml.setAttribute ("sub", (double) s.sub);
    xml.setAttribute ("reverb", (double) s.reverb);
    xml.setAttribute ("level", (double) s.level);
    xml.setAttribute ("bpm", engine.bpm);
    xml.setAttribute ("prog", (int) engine.progSeed);
    xml.setAttribute ("seed", (int) engine.humanSeed);

    for (const auto& b : engine.bars)
    {
        auto* e = xml.createNewChildElement ("BAR");
        e->setAttribute ("base", (int) b.base);
        e->setAttribute ("var", (int) b.var);
        e->setAttribute ("locked", b.locked ? 1 : 0);
        e->setAttribute ("root", b.root);

        juce::String hits;
        for (const auto& h : b.hits)
            hits << juce::String (h.start, 5) << ":" << juce::String (h.length, 5) << ":" << h.pitch << ":" << h.velocity << ":" << h.kind << ";";
        e->setAttribute ("hits", hits);
    }

    copyXmlToBinary (xml, destData);
}

void Purple808Processor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName ("PURPLE808"))
        return;

    const juce::ScopedLock sl (stateLock);

    p808::Settings s;
    auto unit = [&] (const char* name, float fallback) { return juce::jlimit (0.0f, 1.0f, (float) xml->getDoubleAttribute (name, fallback)); };
    s.style      = juce::jlimit (0, (int) p808::kNumStyles - 1, xml->getIntAttribute ("style", s.style));
    s.key        = juce::jlimit (0, 11, xml->getIntAttribute ("key", s.key));
    s.scale      = juce::jlimit (0, (int) p808::kNumScales - 1, xml->getIntAttribute ("scale", s.scale));
    s.octave     = juce::jlimit (-1, 1, xml->getIntAttribute ("octave", s.octave));
    s.density    = unit ("density", s.density);
    s.slides     = unit ("slides", s.slides);
    s.spice      = unit ("spice", s.spice);
    s.movement   = unit ("movement", s.movement);
    s.gate       = unit ("gate", s.gate);
    s.swing      = unit ("swing", s.swing);
    s.stutter    = xml->getIntAttribute ("stutter", 1) != 0;
    s.octaves    = xml->getIntAttribute ("octaves", 1) != 0;
    s.triplets   = xml->getIntAttribute ("triplets", 1) != 0;
    s.gaps       = xml->getIntAttribute ("gaps", 1) != 0;
    s.hostSync   = xml->getIntAttribute ("hostSync", 1) != 0;
    s.kit        = juce::jlimit (0, p808::kNumKits - 1, xml->getIntAttribute ("kit", s.kit));
    s.distortion = unit ("distortion", s.distortion);
    s.tone       = unit ("tone", s.tone);
    s.punch      = unit ("punch", s.punch);
    s.decay      = unit ("decay", s.decay);
    s.glide      = unit ("glide", s.glide);
    s.sub        = unit ("sub", s.sub);
    s.reverb     = unit ("reverb", s.reverb);
    s.level      = unit ("level", s.level);

    std::vector<p808::Bar> loaded;
    for (auto* e : xml->getChildWithTagNameIterator ("BAR"))
    {
        if ((int) loaded.size() >= p808::kMaxBars)
            break;

        p808::Bar b;
        b.base   = (uint32_t) e->getIntAttribute ("base", 1);
        b.var    = (uint32_t) e->getIntAttribute ("var", 1);
        b.locked = e->getIntAttribute ("locked", 0) != 0;
        b.root   = juce::jlimit (-12, 12, e->getIntAttribute ("root", 0));

        for (const auto& token : juce::StringArray::fromTokens (e->getStringAttribute ("hits"), ";", ""))
        {
            const auto f = juce::StringArray::fromTokens (token, ":", "");
            if (f.size() != 5) continue;

            p808::Hit h;
            h.start    = juce::jlimit (0.0, 3.999, f[0].getDoubleValue());
            h.length   = juce::jlimit (-1.0, 8.0, f[1].getDoubleValue());
            h.pitch    = juce::jlimit (-24, 24, f[2].getIntValue());
            h.velocity = juce::jlimit (1, 127, f[3].getIntValue());
            h.kind     = juce::jlimit (0, (int) p808::kGhost, f[4].getIntValue());
            b.hits.push_back (h);
        }
        loaded.push_back (b);
    }

    engine.s = s;
    engine.bpm = p808::Engine::bucket (xml->getDoubleAttribute ("bpm", 140.0));

    if (! loaded.empty())
    {
        engine.bars      = loaded;                  // genau so, wie gespeichert
        engine.s.bars    = (int) loaded.size();
        engine.progSeed  = (uint32_t) xml->getIntAttribute ("prog", 1);
        engine.humanSeed = (uint32_t) xml->getIntAttribute ("seed", 1);
    }
    else
    {
        engine.generate();
    }

    pushSoundParams();
    history.clear();
    historyPos = -1;
    pushHistory();
    publish();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Purple808Processor();
}
