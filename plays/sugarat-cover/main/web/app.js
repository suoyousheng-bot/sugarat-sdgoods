/* Sugarat local photo manager. All network mutations follow an explicit user action. */
(() => {
  'use strict';
  const copy = {
    en: {
      language:'Language',headline:'Carry what you love',subtitle:'Your next cover starts with a photo',showing:'Now on your badge',stored:'Saved on your badge',gallery:'My photos',manage:'Manage',add:'＋ Upload photo',offline:'Photos stay on your badge · Works offline',cropTitle:'Adjust cover',steps:'Choose photo　 ›　 Adjust　 ›　 Upload',circle:'The circle shows what your badge displays. Drag to move.',smaller:'Less',larger:'More',rotate:'Rotate 90°',reset:'Reset crop',preview:'Round screen preview',upload:'Upload to badge',afterUpload:'Choose whether to display it after uploading',show:'Set as cover',delete:'Delete photo',close:'Close',done:'Done',photo:'Photo',current:'Current cover',uploading:'Uploading…',saved:'Photo saved. Tap it to set your cover.',empty:'Your gallery is empty. Upload your first photo.',deleteQuestion:'Delete this photo from your badge?',network:'Connection lost. Keep this page open, reconnect to the badge Wi-Fi and retry.',token:'Open the page QR code shown on your badge.',decode:'This photo cannot be opened. Try a JPEG or PNG.',tooLarge:'Photo is too large. Try another image.',picker:'This Wi-Fi sign-in window cannot open photos. Open http://192.168.4.1 in your browser, or scan the page QR code on your badge.',storage:'Badge storage is unavailable, so photos cannot be added. Restart the badge and reopen this page.',errors:['','Invalid request. Try again.','Choose the photo and crop it again.','Gallery is full. Delete a photo first.','Could not save. Keep the original and retry.','Settings could not be saved. Restart and retry.','Choose another cover before deleting this photo.','Connection timed out. Retry.','Cancelled. Your cover is unchanged.','Session expired. Scan the page QR again.','Wireless unavailable. Restart the badge.','Not enough memory. Close sharing and retry.','Photo is damaged. Upload it again.','Save unconfirmed. Check your gallery before retrying.']
    },
    zh: {
      language:'语言',headline:'把喜欢，带在身上',subtitle:'上传照片，成为你的下一张封面',showing:'设备正在展示',stored:'已保存在徽章中',gallery:'我的图片',manage:'管理',add:'＋ 上传图片',offline:'图片保存在徽章中 · 断网也能展示',cropTitle:'调整封面',steps:'选择照片　 ›　 调整构图　 ›　 上传',circle:'圆圈内就是徽章展示的内容，可以拖动调整',smaller:'缩小',larger:'放大',rotate:'旋转 90°',reset:'重置构图',preview:'圆屏预览',upload:'上传到徽章',afterUpload:'上传后可选择设为当前封面',show:'设为封面',delete:'删除图片',close:'关闭',done:'完成',photo:'图片',current:'当前封面',uploading:'正在上传…',saved:'图片已保存，点击图片即可设为封面',empty:'相册中还没有图片，上传第一张照片吧',deleteQuestion:'从徽章中删除这张图片吗？',network:'连接已断开，请保留此页面，重新连接徽章 Wi-Fi 后重试',token:'请扫描徽章显示的页面二维码进入',decode:'无法打开这张图片，请尝试 JPEG 或 PNG',tooLarge:'图片过大，请换一张图片',picker:'连接 Wi-Fi 后弹出的窗口无法选择图片。请在手机浏览器打开 http://192.168.4.1，或扫描徽章上的页面二维码',storage:'徽章存储不可用，暂时无法添加图片。请重启徽章后重新打开此页面',errors:['','请求无效，请重试','图片无法读取，请重新裁剪上传','相册已满，请先删除一张图片','保存失败，原图片已保留，请重试','设置保存失败，请重启后重试','请先更换当前封面，再删除这张图片','连接超时，请重试','已取消，当前封面保持不变','管理会话已失效，请重新扫描页面二维码','无线连接不可用，请重启设备','内存不足，请退出传图后重试','图片已损坏，请重新上传','未确认是否保存，请先检查相册再重试']
    }
  };
  const $ = id => document.getElementById(id);
  const state = { locale:'en',token:location.hash.slice(1),library:null,urls:new Map(),busy:false,manage:false,detail:null,image:null,zoom:1,angle:0,x:0,y:0,drag:null,failure:'' };
  const t = key => copy[state.locale][key];
  const say = text => { $('notice').textContent=text; $('notice').hidden=!text; };
  function localize() {
    document.documentElement.lang=state.locale;
    document.querySelectorAll('[data-copy]').forEach(node => { node.textContent=t(node.dataset.copy); });
    $('language').value=state.locale;$('language').setAttribute('aria-label',t('language'));
    $('back').setAttribute('aria-label',state.locale==='zh'?'返回相册':'Back to gallery');
    $('zoom').setAttribute('aria-label',state.locale==='zh'?'缩放图片':'Zoom photo');
    $('photos').setAttribute('aria-label',t('gallery'));$('canvas').setAttribute('aria-label',t('preview'));
    $('manage').textContent=t(state.manage?'done':'manage');
    $('upload').textContent=t(state.busy?'uploading':'upload');
    if(state.failure)$('upload-note').textContent=state.failure;
  }
  async function api(path, options={}) {
    const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),20000);
    try {
      const response=await fetch(path,{...options,headers:{...options.headers,'X-Cover-Token':state.token},signal:controller.signal,cache:'no-store'});
      if(!response.ok){let error=1;try{error=(await response.json()).error;}catch(cause){console.warn('Invalid device error response',cause);}throw Error(t('errors')[error]||t('network'));}
      return response;
    } catch(error) { if(error instanceof TypeError||error.name==='AbortError')throw Error(t('network'));throw error; }
    finally {clearTimeout(timeout);}
  }
  async function imageURL(id) {
    if(state.urls.has(id))return state.urls.get(id);
    const response=await api('/api/image/'+id),url=URL.createObjectURL(await response.blob());state.urls.set(id,url);return url;
  }
  async function refresh() {
    const response=await api('/api/state');state.library=await response.json();
    state.locale=state.library.locale==='zh'?'zh':'en';localize();
    $('badge').textContent=state.library.badge;$('count').textContent=state.library.photos.length+' / '+state.library.limit;
    $('current').hidden=!state.library.current;$('choose').disabled=!state.library.writable||state.busy;
    const grid=$('photos');grid.replaceChildren();
    for(const [index,photo] of state.library.photos.entries()) {
      const button=document.createElement('button');button.className='tile';button.type='button';
      const img=document.createElement('img');img.alt=t('photo')+' '+(index+1);img.src=await imageURL(photo.id);button.append(img);
      if(photo.id===state.library.current){const tag=document.createElement('span');tag.className='tag';tag.textContent=t('current');button.append(tag);$('current-image').src=img.src;$('current-image').alt=img.alt;$('current-name').textContent=img.alt;}
      const caption=document.createElement('span');caption.className='caption';caption.textContent=img.alt;button.append(caption);
      button.onclick=()=>openPhoto(photo,index);grid.append(button);
    }
    if(!state.library.photos.length){const empty=document.createElement('p');empty.className='empty';empty.textContent=t('empty');grid.append(empty);}
    if(!state.library.writable)say(t('storage'));else if(state.library.error)say(t('errors')[state.library.error]||t('network'));
  }
  async function openPhoto(photo,index) {
    state.detail=photo;$('detail-image').src=await imageURL(photo.id);$('detail-image').alt=t('photo')+' '+(index+1);$('detail-name').textContent=$('detail-image').alt;
    $('delete-photo').disabled=photo.id===state.library.current||!state.library.writable;
    $('show-photo').disabled=photo.id===state.library.current||!state.library.writable;
    $('delete-photo').hidden=!state.manage;$('show-photo').hidden=state.manage;
    $('photo-dialog').showModal();
  }
  function clampCrop() {
    if(!state.image)return;
    const rotated=state.angle%180!==0,w=rotated?state.image.height:state.image.width,h=rotated?state.image.width:state.image.height;
    const scale=Math.max(360/w,360/h)*state.zoom;
    state.x=Math.max(-(w*scale-360)/2,Math.min((w*scale-360)/2,state.x));
    state.y=Math.max(-(h*scale-360)/2,Math.min((h*scale-360)/2,state.y));
    const ctx=$('canvas').getContext('2d');ctx.save();ctx.clearRect(0,0,360,360);ctx.translate(180+state.x,180+state.y);ctx.rotate(state.angle*Math.PI/180);ctx.scale(scale,scale);ctx.drawImage(state.image,-state.image.width/2,-state.image.height/2);ctx.restore();
  }
  function resetCrop(){state.zoom=1;state.angle=0;state.x=0;state.y=0;$('zoom').value=1;clampCrop();}
  async function choose(file) {
    if(!file)return;if(file.size>30*1024*1024){say(t('tooLarge'));return;}
    let image,url;
    try {url=URL.createObjectURL(file);image=new Image();image.src=url;await image.decode();if(!image.width||image.width*image.height>50000000)throw Error('dimensions');}
    catch(error){say(t('decode'));return;}
    finally {if(url)URL.revokeObjectURL(url);}
    state.image=image;resetCrop();$('library').hidden=true;$('crop-page').hidden=false;window.scrollTo(0,0);
  }
  async function action(operation) {
    if(state.busy)return;state.busy=true;state.failure='';localize();$('upload').disabled=true;$('choose').disabled=true;$('language').disabled=true;
    try {await operation();}
    catch(error){state.failure=error.message;say(error.message);$('upload-note').textContent=error.message;}
    finally {state.busy=false;localize();$('upload').disabled=false;$('choose').disabled=!state.library?.writable;$('language').disabled=false;}
  }
  // Android's Wi-Fi sign-in window shows no picker: nothing happens, or the input reports cancel at once.
  // An opened picker hides or blurs the page first, which clears the pending instruction.
  let pickerCheck=0,pickerAt=0;
  const pickerOpened=()=>{clearTimeout(pickerCheck);pickerCheck=0;};
  const pickerMissing=()=>{pickerOpened();say(t('picker'));};
  ['blur','pagehide'].forEach(type=>window.addEventListener(type,pickerOpened));
  document.addEventListener('visibilitychange',()=>{if(document.hidden)pickerOpened();});
  $('file').addEventListener('cancel',()=>{if(pickerCheck&&Date.now()-pickerAt<500)pickerMissing();else pickerOpened();});
  $('choose').onclick=()=>{
    if(state.busy)return;
    if(/Android/i.test(navigator.userAgent)){pickerOpened();pickerAt=Date.now();pickerCheck=setTimeout(pickerMissing,1500);}
    $('file').click();
  };
  $('file').onchange=()=>{pickerOpened();choose($('file').files[0]);$('file').value='';};
  $('back').onclick=()=>{if(!state.busy){$('library').hidden=false;$('crop-page').hidden=true;state.image=null;}};
  $('zoom').oninput=()=>{state.zoom=Number($('zoom').value);clampCrop();};
  $('rotate').onclick=()=>{if(!state.busy){state.angle=(state.angle+90)%360;state.x=state.y=0;clampCrop();}};
  $('reset').onclick=()=>{if(!state.busy)resetCrop();};
  $('crop').onpointerdown=event=>{if(state.busy)return;state.drag={x:event.clientX,y:event.clientY,ox:state.x,oy:state.y};$('crop').setPointerCapture(event.pointerId);};
  $('crop').onpointermove=event=>{if(!state.drag)return;const scale=360/$('crop').getBoundingClientRect().width;state.x=state.drag.ox+(event.clientX-state.drag.x)*scale;state.y=state.drag.oy+(event.clientY-state.drag.y)*scale;clampCrop();};
  $('crop').onpointerup=$('crop').onpointercancel=()=>{state.drag=null;};
  $('upload').onclick=()=>action(async()=>{
    if(!state.image)return;
    const blob=await new Promise(resolve=>$('canvas').toBlob(resolve,'image/jpeg',.88));
    if(!blob||blob.size>state.library.maxBytes)throw Error(t('tooLarge'));
    await api('/api/photos',{method:'POST',headers:{'Content-Type':'image/jpeg'},body:blob});
    $('library').hidden=false;$('crop-page').hidden=true;state.image=null;await refresh();say(t('saved'));window.scrollTo(0,0);
  });
  $('show-photo').onclick=()=>action(async()=>{await api('/api/current/'+state.detail.id,{method:'PUT'});$('photo-dialog').close();await refresh();say('');});
  $('delete-photo').onclick=()=>{if(confirm(t('deleteQuestion')))action(async()=>{await api('/api/image/'+state.detail.id,{method:'DELETE'});const url=state.urls.get(state.detail.id);if(url)URL.revokeObjectURL(url);state.urls.delete(state.detail.id);$('photo-dialog').close();await refresh();});};
  $('close-dialog').onclick=()=>$('photo-dialog').close();
  $('photo-dialog').onclick=event=>{if(event.target===$('photo-dialog')&&!state.busy)$('photo-dialog').close();};
  $('manage').onclick=()=>{state.manage=!state.manage;localize();};
  $('language').onchange=()=>{const selected=$('language').value;action(async()=>{await api('/api/language/'+selected,{method:'PUT'});await refresh();});};
  window.addEventListener('pagehide',()=>{for(const url of state.urls.values())URL.revokeObjectURL(url);state.urls.clear();});
  window.addEventListener('online',()=>{if(state.token&&!state.busy)refresh().catch(error=>say(error.message));});
  $('choose').disabled=true;localize();if(/^[a-f0-9]{32}$/.test(state.token))refresh().catch(error=>say(error.message));else{say(t('token'));$('choose').disabled=true;}
})();
