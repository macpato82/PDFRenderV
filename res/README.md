# Toolbox resources

`Resources.txt` is the source; `Resources,fae` is what the Toolbox
loads. The viewer is a Toolbox application, as Select's ImageViewer
was, so that the window furniture it needs - an internal bottom-left
toolbar, adjuster arrows, a number range with bump arrows, the Scale
and SaveAs dialogues - comes from the Window module rather than being
drawn by hand.

## Rebuilding

There is no resource compiler that runs on the Mac. `ResGen` in the
DDE builds ResourceFS areas, which is a different thing entirely.
What does the job is **CCres**, which converts a Toolbox resource file
to and from text in both directions, and which runs on RISC OS:

    Run <CCres>.!RunImage <in> <out>

with the direction taken from the input's file type - text (&FFF) in
gives a resource file (&FAE) out. In the emulator:

    Run HostFS::HostFS.$.Programming.AppBasic.Tools.!CCres.!RunImage \
        HostFS::HostFS.$.PDFTest.Res.Src HostFS::HostFS.$.PDFTest.Res.Out

It takes long enough that the command channel times out; the file
still appears. Both files are committed, so this only has to be run
when the resources change.

## ImageViewer-reference.txt

Select's ImageViewer decompiled to the same text format, kept because
it is the layout this is meant to honour and there is no other copy of
it - RISCOS Ltd is gone and the source went with it. CCres stops with
"unhandled gadget class" partway through, on the ImageFileGadget that
only Select had, but everything before that point is intact and that
includes the whole toolbar: two adjusters, a display field, and a
number range with its own bump arrows, which is exactly what the
screenshots show.
