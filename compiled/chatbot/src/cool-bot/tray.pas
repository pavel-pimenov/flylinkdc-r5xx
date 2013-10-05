unit tray;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls;

type
  TTrayForm = class(TForm)
    Image1: TImage;
    Image2: TImage;
  private
    { Private declarations }
    procedure SwitchIcon;
  public
    { Public declarations }
    state: boolean;
    procedure OnWmMouseTray(var message: TMessage); message WM_USER;
    procedure AddTrayIcon(const tip: string);
    procedure BeforeDestruction; override;
  end;

var
  TrayForm: TTrayForm;

implementation

{$R *.DFM}

uses
  ShellApi;

procedure TTrayForm.SwitchIcon;
var
  n: NotifyIconData;
begin
  n.cbSize := sizeof(n);
  n.Wnd := handle;
  n.uID := 0;
  if (state) then n.hIcon := Image1.Picture.Icon.Handle
  else n.hIcon := Image2.Picture.Icon.Handle;
  n.uFlags := NIF_ICON;
  Shell_NotifyIcon(NIM_MODIFY, @n);
end;

procedure TTrayForm.AddTrayIcon(const tip: string);
var
  n: NotifyIconData;
begin
  n.cbSize := sizeof(n);
  n.Wnd := handle;
  n.uID := 0;
  n.hIcon := Image1.Picture.Icon.Handle;
  n.uCallbackMessage := WM_USER;
  n.uFlags := NIF_ICON or NIF_TIP or NIF_MESSAGE;
  CopyMemory(@n.szTip, pchar(tip), sizeof(n.szTip));
  Shell_NotifyIcon(NIM_ADD, @n);
  state := true;
end;

procedure TTrayForm.BeforeDestruction;
var
  n: NotifyIconData;
begin
  n.cbSize := sizeof(n);
  n.Wnd := handle;
  n.uID := 0;
  n.uFlags := 0;
  Shell_NotifyIcon(NIM_DELETE, @n);
end;

procedure TTrayForm.OnWmMouseTray(var message: TMessage);
begin
  if (message.LParam <> WM_LBUTTONDOWN) and (message.LParam <> WM_RBUTTONDOWN) then exit;
  state := not state;
  SwitchIcon;
end;

initialization
  TrayForm := TTrayForm.Create(nil);
  TrayForm.AddTrayIcon('Chat Bot for ApexDC++ mod');
finalization
  TrayForm.Free;
  TrayForm := nil;
end.
