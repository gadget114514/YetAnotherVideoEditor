import type { Command } from './Command.js';

export class CommandStack {
  private undoList_: Command[] = [];
  private redoList_: Command[] = [];
  private cleanIndex_: number = 0;

  push(cmd: Command): void {
    cmd.redo();
    if (this.undoList_.length > 0) {
      const top = this.undoList_[this.undoList_.length - 1]!;
      if (top.mergeWith && top.mergeWith(cmd)) {
        return;
      }
    }
    this.undoList_.push(cmd);
    this.redoList_ = [];
  }

  undo(): void {
    const cmd = this.undoList_.pop();
    if (cmd) {
      cmd.undo();
      this.redoList_.push(cmd);
    }
  }

  redo(): void {
    const cmd = this.redoList_.pop();
    if (cmd) {
      cmd.redo();
      this.undoList_.push(cmd);
    }
  }

  get canUndo(): boolean {
    return this.undoList_.length > 0;
  }

  get canRedo(): boolean {
    return this.redoList_.length > 0;
  }

  get isClean(): boolean {
    return this.undoList_.length === this.cleanIndex_;
  }

  setClean(): void {
    this.cleanIndex_ = this.undoList_.length;
  }

  clear(): void {
    this.undoList_ = [];
    this.redoList_ = [];
    this.cleanIndex_ = 0;
  }
}
