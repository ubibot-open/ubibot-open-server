function csvCell(v: unknown): string {
  return `"${String(v).replace(/"/g, '""')}"`
}

export function toCsv(rows: unknown[][]): string {
  return rows.map((row) => row.map(csvCell).join(',')).join('\n')
}

// parseCsv reads RFC4180-ish CSV back into rows of strings — the import
// counterpart to toCsv above. Handles quoted fields (embedded commas,
// newlines, and doubled `""` for a literal quote) as well as bare
// unquoted fields; \r\n and \n line endings both work. A trailing blank
// line is dropped rather than producing an empty final row, so pasting
// text with a trailing newline (the common case) doesn't need trimming
// first.
export function parseCsv(text: string): string[][] {
  const rows: string[][] = []
  let row: string[] = []
  let field = ''
  let inQuotes = false
  let i = 0
  const pushField = () => {
    row.push(field)
    field = ''
  }
  const pushRow = () => {
    pushField()
    rows.push(row)
    row = []
  }

  while (i < text.length) {
    const c = text[i]
    if (inQuotes) {
      if (c === '"') {
        if (text[i + 1] === '"') {
          field += '"'
          i += 2
          continue
        }
        inQuotes = false
        i += 1
        continue
      }
      field += c
      i += 1
      continue
    }
    if (c === '"') {
      inQuotes = true
      i += 1
      continue
    }
    if (c === ',') {
      pushField()
      i += 1
      continue
    }
    if (c === '\r') {
      i += 1
      continue
    }
    if (c === '\n') {
      pushRow()
      i += 1
      continue
    }
    field += c
    i += 1
  }
  // Final field/row, unless the input ended cleanly on a newline (already
  // flushed by the '\n' branch above) or was empty to begin with.
  if (field !== '' || row.length > 0) {
    pushRow()
  }
  return rows
}

// Triggers a browser download of csv as filename. The leading BOM is what
// makes Excel (still the most likely consumer) detect UTF-8 instead of
// mangling any non-ASCII device names/field keys.
export function downloadCsv(filename: string, csv: string) {
  const blob = new Blob(['﻿' + csv], { type: 'text/csv;charset=utf-8;' })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
}
