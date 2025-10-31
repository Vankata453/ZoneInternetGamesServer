Option Explicit
Const adTypeBinary = 1
Const adTypeText = 2
Const adSaveCreateOverWrite = 2


' [CONSTANTS]

const gamesFolder = "C:\Program Files\MSN Gaming Zone\WINDOWS"

' "EXE filename : hostname offset : byte length of hostname"
Dim games : games = Array( _
  "BCKGZM.EXE:109E6:96", _
  "CHKRZM.EXE:109E2:92", _
  "HRTZZM.EXE:109DE:88", _
  "RVSEZM.EXE:109E0:90", _
  "SHVLZM.EXE:109DE:88" _
)


' [MAIN]

Dim isCommandLine : isCommandLine = InStr(LCase(WScript.FullName), "cscript.exe") > 0

Dim fso : Set fso = CreateObject("Scripting.FileSystemObject")

Dim prompt : prompt = "Enter a host (max. 44 characters) (leave blank to skip): "
Dim userHostHex : userHostHex = GetUserStringInputAsUnicodeHex(prompt, 44)

prompt = "Enter a valid port (0-65535) (leave blank to skip): "
Dim userPortHex : userPortHex = GetUserUInt16InputHex(prompt)

Dim game, parts, name, offset, byteLen, filePath, backupFilePath, fileStream

' GAME EXECUTABLES
If isCommandLine Then
  WScript.Echo "" ' Newline
End If
If userHostHex <> "" Then
  For Each game In games
    parts = Split(game, ":")
    If UBound(parts) < 2 Then
      WScript.Echo "ERROR: Invalid game spec: " & game
      WScript.Quit 1
    Else
      name = Trim(parts(0))
      offset = CLng("&H" & Trim(parts(1)))
      byteLen = Trim(parts(2))
      filePath = fso.BuildPath(gamesFolder, name)
      If Not fso.FileExists(filePath) Then
        WScript.Echo "WARNING: " & filePath & " not found, skipping!"
      Else
        If isCommandLine Then
          WScript.Echo "Patching " & filePath & "..."
        End If

        ' Backup
        backupFilePath = filePath & ".bak"
        On Error Resume Next
        fso.CopyFile filePath, backupFilePath, True
        On Error GoTo 0

        ' Open file stream
        Set fileStream = CreateObject("ADODB.Stream")
        fileStream.Type = adTypeBinary
        fileStream.Open
        fileStream.LoadFromFile filePath

        ' Patch offset in stream
        ApplyPatch fileStream, offset, PadHexWithZeros(userHostHex, byteLen)

        ' Save to file and close stream
        fileStream.SaveToFile filePath, adSaveCreateOverWrite
        fileStream.Close
        Set fileStream = Nothing
        If isCommandLine Then
          WScript.Echo "Successfully patched and saved " & filePath & "!"
        End If
      End If
    End If
    If isCommandLine Then
      WScript.Echo "" ' Newline
    End If
  Next
Else
  If isCommandLine Then
    WScript.Echo "Skipped host patching."
    WScript.Echo "" ' Newline
  End If
End If  

' CMNCLIM.DLL
If userPortHex <> "" Then
  name = "CMNCLIM.DLL"
  offset = CLng("&H23967")
  filePath = fso.BuildPath(gamesFolder, name)
  If Not fso.FileExists(filePath) Then
    WScript.Echo "WARNING: " & filePath & " not found, skipping!"
  Else
    If isCommandLine Then
      WScript.Echo "Patching " & filePath & "..."
    End If

    ' Backup
    backupFilePath = filePath & ".bak"
    On Error Resume Next
    fso.CopyFile filePath, backupFilePath, True
    On Error GoTo 0

    ' Open file stream
    Set fileStream = CreateObject("ADODB.Stream")
    fileStream.Type = adTypeBinary
    fileStream.Open
    fileStream.LoadFromFile filePath

    ' Patch offset in stream
    ApplyPatch fileStream, offset, PadHexWithZeros(userPortHex, 4) ' 4 bytes, as int32 is used for the port

    ' Save to file and close stream
    fileStream.SaveToFile filePath, adSaveCreateOverWrite
    fileStream.Close
    Set fileStream = Nothing
    If isCommandLine Then
      WScript.Echo "Successfully patched and saved " & filePath & "!"
    End If
  End If
Else
  If isCommandLine Then
    WScript.Echo "Skipped port patching."
  End If
End If
If isCommandLine Then
  WScript.Echo "" ' Newline
End If

WScript.Echo "Internet Games Patcher finished successfully!"


' [HELPERS]

Function GetUserInput(msg)
  ' Try console first (cscript). If not available, fallback to InputBox (wscript).
  On Error Resume Next
  WScript.StdOut.Write msg
  Dim s
  s = ""
  If Err.Number = 0 Then
    s = WScript.StdIn.ReadLine()
  Else
    s = InputBox(msg, "Internet Games Patcher")
  End If
  On Error GoTo 0
  GetUserInput = s
End Function

Function GetUserUInt16InputHex(prompt)
  Dim inputStr, value, hexStr, lo, hi

  Do
    inputStr = GetUserInput(prompt)
    If inputStr = "" Then
      GetUserUInt16InputHex = ""   ' user skipped
      Exit Function
    End If

    If IsNumeric(inputStr) Then
      value = CLng(inputStr)

      ' Validate unsigned 16-bit range
      If value >= 0 And value <= 65535 Then
        lo = value And &HFF
        hi = (value \ &H100) And &HFF
        hexStr = Right("00" & Hex(lo), 2) & Right("00" & Hex(hi), 2)
        GetUserUInt16InputHex = hexStr
        Exit Function
      Else
        WScript.Echo "Value out of UInt16 range (0 to 65535). Try again."
      End If
    Else
      WScript.Echo "Invalid number. Try again."
    End If
  Loop
End Function

Function GetUserStringInputAsUnicodeHex(prompt, limit)
  Dim inputStr, hexStr, i, ch

  Do
    inputStr = GetUserInput(prompt)
    If inputStr = "" Then
      GetUserStringInputAsUnicodeHex = ""  ' user skipped
      Exit Function
    End If

    If Len(inputStr) > limit Then
        WScript.Echo "Input too long (max. " & limit & " characters). Try again."
    Else
      hexStr = ""
      For i = 1 To Len(inputStr)
        ch = Mid(inputStr, i, 1)
        hexStr = hexStr & Right("00" & Hex(Asc(ch)), 2) & "00"
      Next
      ' Remove spaces, just in case
      hexStr = Replace(hexStr, " ", "")
      GetUserStringInputAsUnicodeHex = hexStr
      Exit Function
    End If
  Loop
End Function

Function PadHexWithZeros(hexStr, totalBytes)
  Dim currentBytes, needed, paddedHexStr, i
  ' Calculate how many bytes are already in hexStr
  currentBytes = Len(hexStr) / 2

  If currentBytes >= totalBytes Then
    PadHexWithZeros = hexStr ' Already long enough
    Exit Function
  End If

  needed = totalBytes - currentBytes
  paddedHexStr = hexStr
  For i = 1 To needed
    paddedHexStr = paddedHexStr & "00"
  Next

  PadHexWithZeros = paddedHexStr
End Function

Function HexToString(h)
  Dim j, s
  s = ""
  For j = 1 To Len(h) Step 2
    s = s & ChrW(Eval("&H" & Mid(h, j, 2)))
  Next
  HexToString = s
End Function

Sub ApplyPatch(targetStream, offset, hexStr)
  If Len(hexStr) = 0 Then Exit Sub
    If (Len(hexStr) Mod 2) <> 0 Then
      WScript.Echo "ERROR: Bad hex string length at offset " & offset
      WScript.Quit 1
    Exit Sub
  End If

  Dim small : Set small = CreateObject("ADODB.Stream")
  Dim conv  : Set conv  = CreateObject("ADODB.Stream")

  conv.Type = adTypeText
  conv.Charset = "iso-8859-1" ' Handles bytes 00–FF safely
  conv.Open
  conv.WriteText HexToString(hexStr)
  conv.Position = 0

  small.Type = adTypeBinary
  small.Open

  conv.CopyTo small
  conv.Close
  Set conv = Nothing

  small.Position = 0
  targetStream.Position = offset
  small.CopyTo targetStream
  small.Close
  Set small = Nothing

  ' WScript.Echo "Patched offset 0x" & Hex(offset) & " (" & (Len(hexStr) \ 2) & " bytes)"
End Sub
