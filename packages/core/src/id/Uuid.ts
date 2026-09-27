export type Uuid = string & { readonly __brand: unique symbol };

export function createUuid(): Uuid {
  return crypto.randomUUID() as Uuid;
}

export function asUuid(id: string): Uuid {
  return id as Uuid;
}
