//5 has graphics
$SP,"<1>",BI=1$





U0 Song()
{
  CTask *task;
  Fs->task_end_cb=&SndTaskEndCB;
  MusicSettingsRst;
  music.tempo=2.5;
  try {
    task=PopUpViewPrint("$LK,"Philippians,4:8",A="BF:Philippians,4:8"$\n");
    TaskWait(task);
    WinHorz(Fs->win_left,TEXT_COLS-10,task);
    WinVert(Fs->win_bottom+3,Fs->win_bottom+15,task);
    "$$WW+H,1$$";
    while (!ScanKey) {
      Play("5eEG4qA5CetCFGeEFqGetGFE4qA",
	    "Look\0at\0that!\0\0\0\0Dew\0\0\0"
	    "on\0flow\0\0\0ers.\n\0");
      Play("5eEG4qA5CetCFGeEFqGetGFE4qA",
	    "Look\0at\0that!\0\0\0\0Dew\0\0\0"
	    "on\0flow\0\0\0ers.\n\0");
      Sprite($IB,"<1>",BI=1$);
      "$$CM,0,7$$";
      Play("5eDEqE4A5EFCCetC4A5D",
	    "Pret\0ty\0when\0\0they\0catch\0the\0light.\n\0\0\0\0");
      Play("5eDEqE4A5EFCCetC4A5D",
	    "Pret\0ty\0when\0\0they\0catch\0the\0light.\n\0\0\0\0");
      "$$GREEN$$(It's easy to be dark and profound all the time in arrogance."
	    "Sometimes, I search for self-evident goodness.)\n$$FG$$";
    }
  } catch
    PutExcept;
  Snd;
}

Song;
        �     	$       "   L�0J�$��$a			   (   $   I�0a�(J�(J� 	   $      I�ͶM�	   %      ��$�
	   %      ���$�[�	   #      I 	%   $   #   IEI���FQ��I	   #      ���		   ,      $�m	
   ,      $�m3	   .      %f���	   2      ���a�
/      1   ����
/      2   ����	   /      &	   .      I$	   1      $�e�	   3      ��(\=�˒0$	   5      &�$v�		   (      [�$	   (      $�$��	   )      Y�,	   '      ��-K	     !   3��<I�$I�$I�$I���mۺ�۶m޶m�6 2l۶�$I�$I��f�$IR��IH�$I�ܺy6I�$#J��m��5I�)*۶�ۼ�ݶm۶m۶m�mۆmۖ%I�m�ܒ?ϓ�I�$I�$I�$i��I�$I�$E�$�H��$۶mݶ۶y&y�$I�"ù�k�޶m۶m۶m۶m۶m۶m۶m۶a l	�C���m�f�6I��i	I   
      �(Q��$ɟ�����nI��[;Ϯm�"C�	w   .      ���$I��(��D�^ݒ"E��(�m�v�$ɶm۶m۶m۶m�m۞=	         I	%   E   &   $I�<)F�&I�m�6
	   1      d���	   $      L��		   '      d��	   (      &�]{N�(	   5      "%Fںu	
   9       $I�$	         n�m[ 		         LBE	   =   %   $I 