object Form4: TForm4
  Left = 0
  Top = 0
  BiDiMode = bdLeftToRight
  BorderIcons = [biSystemMenu]
  BorderStyle = bsSingle
  Caption = 'Credential Manager'
  ClientHeight = 204
  ClientWidth = 344
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  ParentBiDiMode = False
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object ListBox1: TListBox
    Left = 16
    Top = 8
    Width = 121
    Height = 186
    ItemHeight = 13
    TabOrder = 0
  end
  object ListBox2: TListBox
    Left = 207
    Top = 8
    Width = 121
    Height = 186
    ItemHeight = 13
    TabOrder = 1
  end
  object Button1: TButton
    Left = 143
    Top = 56
    Width = 58
    Height = 25
    Caption = '->'
    TabOrder = 2
    OnClick = Button1Click
  end
  object Button2: TButton
    Left = 143
    Top = 87
    Width = 58
    Height = 25
    Caption = '<-'
    TabOrder = 3
    OnClick = Button2Click
  end
end
