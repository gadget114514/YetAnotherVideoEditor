import { describe, it, expect } from 'vitest';
import { Project, VideoClip, Timebase } from '@yave/core';
import { ExportJob } from '../src/export/ExportJob.js';

describe('ExportJob', () => {
  it('runs offline export loop and reports progress', async () => {
    const project = new Project();
    project.setTimebase(Timebase.Fps30);
    const track = project.timeline.appendTrack('video');
    const clip = new VideoClip();
    clip.setRange({ start: 0, duration: 60 }); // 2 seconds at 30fps
    track.insertClip(clip);

    const progressReports: number[] = [];
    const job = new ExportJob(project, {
      width: 1280,
      height: 720,
      onProgress: (p) => {
        progressReports.push(p);
      },
    });

    const result = await job.run();
    expect(result.ok).toBe(true);
    expect(result.totalFrames).toBe(60);
    expect(result.durationSeconds).toBe(2);
    expect(result.buffer).toBeDefined();
    expect(result.buffer!.byteLength).toBeGreaterThan(0);
    expect(progressReports.length).toBeGreaterThan(0);
    expect(progressReports[progressReports.length - 1]).toBe(1.0);
  });

  it('supports abort signal to cancel export', async () => {
    const project = new Project();
    const track = project.timeline.appendTrack('video');
    const clip = new VideoClip();
    clip.setRange({ start: 0, duration: 300 });
    track.insertClip(clip);

    const controller = new AbortController();
    const job = new ExportJob(project, {
      signal: controller.signal,
      onProgress: (p) => {
        if (p > 0.05) {
          controller.abort();
        }
      },
    });

    // Abort before starting
    controller.abort();
    const result = await job.run();
    expect(result.ok).toBe(false);
    expect(result.error).toContain('aborted');
  });
});
