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
Dim userHost : userHost = GetUserStringInputAsUnicode(prompt, 44)

prompt = "Enter a valid port (0-65535) (leave blank to skip): "
Dim userPortHex : userPortHex = GetUserUInt16InputHex(prompt)

Dim game, parts, name, offset, byteLen, filePath, backupFilePath, fileStream

' GAME EXECUTABLES
If isCommandLine Then
  WScript.Echo "" ' Newline
End If
If userHost <> "" Then
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
        fileStream.Type = adTypeText
        fileStream.Charset = "iso-8859-1"
        fileStream.Open
        fileStream.LoadFromFile filePath

        ' Patch offset in stream
        fileStream.Position = offset
        fileStream.WriteText PadStringWithZeros(userHost, byteLen)

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
    ' Prepare a byte representation of the port number to be written to the DLL.
    ' This works around VBScript’s inability to create a SAFEARRAY compatible
    ' with ADODB.Stream.Write by using MSXML, which returns a SAFEARRAY when
    ' decoding a hex value.
    Dim xml, xmlPortNode
    On Error Resume Next
    Set xml = CreateObject("Microsoft.XMLDOM")
    If Err.Number <> 0 Then
      WScript.Echo "ERROR: Couldn't create Microsoft.XMLDOM object for Hex conversion, required for patching port!"
      WScript.Quit 1
    End If
    On Error GoTo 0
    Set xmlPortNode = xml.createElement("p")
    xmlPortNode.DataType = "bin.hex"
    xmlPortNode.Text = userPortHex & "0000" ' Add 2 more bytes to make it 4, as the port is represented as an int32.
    ' xmlPortNode.nodeTypedValue is now a SAFEARRAY that contains the port, represented in bytes.

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
    fileStream.Position = offset
    fileStream.Write xmlPortNode.nodeTypedValue

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

Function GetUserStringInputAsUnicode(prompt, limit)
  Dim inputStr
  Do
    inputStr = GetUserInput(prompt)
    If inputStr = "" Then
      GetUserStringInputAsUnicode = "" ' User skipped input
      Exit Function
    End If

    If Len(inputStr) > limit Then
        WScript.Echo "Input too long (max. " & limit & " characters). Try again."
    Else
      ' Create a Unicode-style byte representation of the input string
      ' by inserting a zero (empty) byte after each character.
      Dim unicodeStr, i
      For i = 1 To Len(inputStr)
        unicodeStr = unicodeStr & Mid(inputStr, i, 1) & Chr(0)
      Next
      GetUserStringInputAsUnicode = unicodeStr
      Exit Function
    End If
  Loop
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

Function PadStringWithZeros(str, amount)
  If Len(str) >= amount Then
    PadStringWithZeros = str
    Exit Function
  End If

  Dim padStr, i
  padStr = str
  For i = 1 To amount - Len(padStr)
    padStr = padStr & Chr(0)
  Next
  PadStringWithZeros = padStr
End Function
