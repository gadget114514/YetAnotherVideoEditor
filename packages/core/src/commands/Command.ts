export interface Command {
  readonly name: string;
  redo(): void;
  undo(): void;
  mergeWith?(nextCommand: Command): boolean;
}
