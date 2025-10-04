export const sdlCallbackInit: (d) => void;
export const sdlLaunchMain: (lib: string, func: string) => number;
export const sdlKeyEvent: (scancode: number, type: number) => number;
export const sdlTextAppend: (str: string) => number;
export const sdlTextEditing: (str: string, loc: number, length: number) => number;