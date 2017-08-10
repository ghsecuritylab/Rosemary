// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_manager_impl.h"

#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/cursors/webcursor.h"
#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/render_thread_impl.h"
#include "ui/gfx/point.h"

namespace content {

BrowserPluginManagerImpl::BrowserPluginManagerImpl(RenderViewImpl* render_view)
    : BrowserPluginManager(render_view) {
}

BrowserPluginManagerImpl::~BrowserPluginManagerImpl() {
}

BrowserPlugin* BrowserPluginManagerImpl::CreateBrowserPlugin(
    RenderViewImpl* render_view,
    blink::WebFrame* frame,
    scoped_ptr<BrowserPluginDelegate> delegate) {
  return new BrowserPlugin(render_view, frame, delegate.Pass());
}

bool BrowserPluginManagerImpl::Send(IPC::Message* msg) {
  return RenderThread::Get()->Send(msg);
}

bool BrowserPluginManagerImpl::OnMessageReceived(
    const IPC::Message& message) {
  if (BrowserPlugin::ShouldForwardToBrowserPlugin(message)) {
    int browser_plugin_instance_id = browser_plugin::kInstanceIDNone;
    // All allowed messages must have |browser_plugin_instance_id| as their
    // first parameter.
    PickleIterator iter(message);
    bool success = iter.ReadInt(&browser_plugin_instance_id);
    DCHECK(success);
    BrowserPlugin* plugin = GetBrowserPlugin(browser_plugin_instance_id);
    if (plugin && plugin->OnMessageReceived(message))
      return true;
  }

  return false;
}

void BrowserPluginManagerImpl::DidCommitCompositorFrame() {
  IDMap<BrowserPlugin>::iterator iter(&instances_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->DidCommitCompositorFrame();
    iter.Advance();
  }
}

}  // namespace content
