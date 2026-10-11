# SPDX-License-Identifier: MIT
"""Behavioral browser checks against the ordinary Pascal demo's visible output."""
import argparse
import json
import re
from pathlib import Path
from playwright.sync_api import expect, sync_playwright, TimeoutError as PlaywrightTimeoutError

parser = argparse.ArgumentParser()
parser.add_argument('--url', default='http://127.0.0.1:8765/')
parser.add_argument('--chromium', default='/usr/bin/chromium')
args = parser.parse_args()
with sync_playwright() as p:
    browser = p.chromium.launch(headless=True, executable_path=args.chromium,
                                args=['--no-sandbox'])
    page = browser.new_page(viewport={'width': 1100, 'height': 900})
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.goto(args.url)
    page.wait_for_selector('#status[data-state=ready]')
    page.wait_for_function('Number(document.querySelector("#lcl").dataset.frames)>0')

    # A real LCL memo is backed by a DOM textarea. Composition is held outside
    # the Pascal value until compositionend, then the committed Unicode value is
    # delivered before Ctrl+Enter is routed through the LCL key callback.
    memo = page.locator('#lcl-dom textarea')
    expect(memo).to_have_value('seed\n')
    composition = page.get_by_label('Composition status')
    value = page.get_by_label('Composition value')
    saved_text = page.get_by_label('Saved text')
    change_count = page.get_by_label('Memo change count')
    expect(composition).to_have_value('Composition: idle')
    expect(saved_text).to_have_value('Saved: none')
    expect(change_count).to_have_value('Changes: 0')

    # Focus handlers can queue async application work. It must run before the
    # browser focus callback returns, without a later key or input event.
    focus_result = page.evaluate('''() => {
        document.querySelector('#lcl-dom textarea').focus();
        document.querySelector('#lcl-dom input[aria-label="Composition status"]').focus();
        return document.querySelector('#lcl-dom input[aria-label="Saved text"]').value;
    }''')
    assert focus_result == 'Focus callback completed', focus_result

    # LCL Tab traversal must move browser focus too. If it leaves DOM focus on
    # the textarea, typing after the move still changes the memo invisibly.
    memo.focus()
    page.keyboard.press('Tab')
    destroy_on_change = page.get_by_role('button', name='Destroy on change', exact=True)
    expect(destroy_on_change).to_be_focused()
    page.keyboard.press('x')
    expect(memo).to_have_value('seed\n')
    page.keyboard.press('Shift+Tab')
    expect(memo).to_be_focused()

    memo.evaluate('''el => {
        el.focus();
        el.dispatchEvent(new CompositionEvent('compositionstart', {bubbles:true}));
        el.value = 'かな😀';
        el.dispatchEvent(new InputEvent('input', {bubbles:true, inputType:'insertCompositionText',
            data:'かな😀', isComposing:true}));
    }''')
    expect(composition).to_have_value('Composition: active')
    expect(value).to_have_value('Value: seed<LF>')
    page.get_by_role('button', name='Save text', exact=True).click()
    expect(saved_text).to_have_value('Save pending')
    memo.evaluate("el => el.dispatchEvent(new CompositionEvent('compositionend', {bubbles:true, data:'かな😀'}))")
    expect(composition).to_have_value('Composition: idle')
    expect(value).to_have_value('Value: かな😀')
    expect(saved_text).to_have_value('Saved: かな😀')
    expect(change_count).to_have_value('Changes: 1')
    memo.focus()
    page.keyboard.press('Control+Enter')
    expect(composition).to_have_value('Ctrl+Enter routed')
    page.get_by_role('button', name='Set memo text', exact=True).click()
    expect(memo).to_have_value('setter')
    expect(value).to_have_value('Value: setter')
    expect(change_count).to_have_value('Changes: 1')
    page.get_by_role('button', name='Save text', exact=True).click()
    expect(saved_text).to_have_value('Saved: setter')
    page.get_by_role('button', name='Set memo text', exact=True).click()
    expect(memo).to_have_value('setter\n')
    expect(value).to_have_value('Value: setter<LF>')
    expect(change_count).to_have_value('Changes: 1')
    old_memo = memo.element_handle()
    page.get_by_role('button', name='Recreate memo', exact=True).click()
    expect(memo).to_have_value('setter\n')
    expect(value).to_have_value('Value: setter<LF>')
    expect(change_count).to_have_value('Changes: 1')
    page.get_by_role('button', name='Save text', exact=True).click()
    expect(saved_text).to_have_value('Saved: setter<LF>')
    old_memo.evaluate('''el => {
        el.dispatchEvent(new CompositionEvent('compositionstart', {bubbles:true}));
        el.dispatchEvent(new CompositionEvent('compositionend', {bubbles:true}));
    }''')
    expect(composition).to_have_value('Ctrl+Enter routed')
    expect(memo).to_have_value('setter\n')
    expect(change_count).to_have_value('Changes: 1')
    for change_number, memo_text in enumerate(
            ['', 'alpha', 'alpha\nbeta', 'alpha\n', 'alpha\nbeta\n'], 2):
        memo.evaluate('''(el, value) => {
            el.value = value;
            el.dispatchEvent(new Event('input', {bubbles:true}));
        }''', memo_text)
        expect(memo).to_have_value(memo_text)
        value_text = memo_text.replace('\r', '<CR>').replace('\n', '<LF>')
        expect(value).to_have_value('Value: ' + value_text)
        page.get_by_role('button', name='Save text', exact=True).click()
        expect(saved_text).to_have_value('Saved: ' + value_text)
        expect(change_count).to_have_value('Changes: ' + str(change_number))
    page.get_by_role('button', name='Destroy on change', exact=True).click()
    old_memo = memo.element_handle()
    old_memo.evaluate('''el => {
        el.dispatchEvent(new CompositionEvent('compositionstart', {bubbles:true}));
        el.value = 'destroy';
        el.dispatchEvent(new CompositionEvent('compositionend', {bubbles:true, data:'destroy'}));
    }''')
    expect(value).to_have_value('Memo destroyed')
    expect(change_count).to_have_value('Changes: 7')
    old_memo.evaluate('''el => {
        el.dispatchEvent(new CompositionEvent('compositionstart', {bubbles:true}));
        el.dispatchEvent(new CompositionEvent('compositionend', {bubbles:true}));
    }''')
    expect(composition).to_have_value('Composition: idle')
    expect(change_count).to_have_value('Changes: 7')

    # A painted combo has no overlay element of its own. Leaving the DOM edit
    # focused would make the host discard ArrowDown before it reaches LCL.
    focus_source = page.get_by_label('Composition value')
    focus_source.focus()
    expect(focus_source).to_be_focused()
    page.keyboard.press('Tab')
    expect(saved_text).to_have_value('Painted combo focused')
    expect(focus_source).not_to_be_focused()
    page.keyboard.press('ArrowDown')
    expect(saved_text).to_have_value('Painted combo: Bold')
    expect(focus_source).to_have_value('Memo destroyed')

    def open_painted_combo_picker():
        canvas = page.locator('#lcl')
        canvas_box = canvas.bounding_box()
        canvas_width, canvas_height = canvas.evaluate('(el) => [el.width, el.height]')
        page.mouse.click(canvas_box['x'] + 686 * canvas_box['width'] / canvas_width,
                         canvas_box['y'] + 139 * canvas_box['height'] / canvas_height)
        picker = page.get_by_role('dialog', name='Choose item')
        expect(picker).to_be_visible()
        return picker

    picker = open_painted_combo_picker()
    expect(picker.locator('option:checked')).to_have_text('Bold')
    picker.get_by_role('button', name='Cancel').click()
    expect(saved_text).to_have_value('Painted combo: Bold')

    picker = open_painted_combo_picker()
    expect(picker.locator('option:checked')).to_have_text('Bold')
    picker.get_by_role('button', name='OK').click()
    expect(saved_text).to_have_value('Painted combo: Bold')
    picker = open_painted_combo_picker()
    expect(picker.locator('option:checked')).to_have_text('Bold')
    picker.get_by_role('button', name='Cancel').click()

    def frame():
        return page.locator('#lcl').get_attribute('data-frames')

    def painted_after(old):
        page.wait_for_function('(old) => document.querySelector("#lcl").dataset.frames !== old', arg=old)

    def pixel(x, y):
        return page.evaluate('([x,y]) => Array.from(document.querySelector("#lcl").getContext("2d").getImageData(x,y,1,1).data)', [x, y])

    def wait_pixel(x, y, color):
        page.wait_for_function('''([x,y,color]) => {
            const p = document.querySelector('#lcl').getContext('2d').getImageData(x,y,1,1).data;
            return color.every((v,i) => p[i] === v);
        }''', arg=[x, y, color])

    count = page.get_by_role('button', name=re.compile(r'^Count: \d+$'))
    count.click()
    expect(count).to_have_text('Count: 1')
    count.focus()
    page.keyboard.press('Enter')
    expect(count).to_have_text('Count: 2')
    assert count.evaluate('(el) => el === document.activeElement'), 'Repaint stole DOM focus'

    # The LFM/Pascal specifies light blue RGB(175,207,234), not a host-generated shape.
    assert pixel(158, 263) == [175, 207, 234, 255], pixel(158, 263)
    checkbox = page.get_by_role('checkbox', name='Filled shape')
    old = frame()
    checkbox.focus()
    page.keyboard.press('Space')
    painted_after(old)
    wait_pixel(158, 263, [255, 255, 255, 255])
    assert not checkbox.is_checked()
    assert pixel(158, 263) == [255, 255, 255, 255]
    old = frame()
    checkbox.click()
    painted_after(old)
    wait_pixel(158, 263, [175, 207, 234, 255])
    assert checkbox.is_checked()
    assert pixel(158, 263) == [175, 207, 234, 255]

    box = page.locator('#lcl').bounding_box()
    old = frame()
    page.mouse.click(box['x']+464, box['y']+264)
    painted_after(old)
    assert pixel(464, 264) == [38, 122, 56, 255], pixel(464, 264)
    old = frame()
    page.get_by_role('button', name='Reset', exact=True).click()
    painted_after(old)
    assert count.inner_text() == 'Count: 0'
    assert pixel(464, 264) == [255, 255, 255, 255]

    before = page.evaluate('({live:jobHost.live(),slots:jobHost.slots()})')
    count.focus()
    for _ in range(100):
        page.keyboard.press('Enter')
    expect(count).to_have_text('Count: 100')
    after = page.evaluate('({live:jobHost.live(),slots:jobHost.slots()})')
    assert after['live'] == before['live'], (before, after)
    assert after['slots'] <= before['slots']+2, (before, after)

    # Exercise the platform resize boundary; the LFM's Anchors must grow the paintbox.
    old = frame()
    page.evaluate('lclDemo.lcl_resize(800,520)')
    page.wait_for_function('document.querySelector("#lcl").width === 800 && document.querySelector("#lcl").height === 520')
    painted_after(old)
    assert page.locator('#lcl').get_attribute('width') == '800'
    assert pixel(760, 490) == [255, 255, 255, 255], pixel(760, 490)

    save_button = page.get_by_role('button', name='Save file', exact=True)
    save_again = page.get_by_role('button', name='Save again', exact=True)
    atomic = page.get_by_role('checkbox', name='Atomic rename', exact=True)
    fail_rename = page.get_by_role('checkbox', name='Fail rename', exact=True)
    payload = b'WASI browser save payload\n'
    for name, expected_name, use_atomic in [
            ('direct', 'direct.txt', False), ('atomic.txt', 'atomic.txt', True)]:
        atomic.set_checked(use_atomic)
        save_button.click()
        dialog = page.get_by_role('dialog')
        dialog.get_by_label('File name').fill(name)
        with page.expect_download() as pending:
            dialog.get_by_role('button', name='OK', exact=True).click()
        saved = pending.value
        assert saved.suggested_filename == expected_name, saved.suggested_filename
        assert Path(saved.path()).read_bytes() == payload
        with page.expect_download() as repeated:
            save_again.click()
        repeated_save = repeated.value
        assert repeated_save.suggested_filename == expected_name, repeated_save.suggested_filename
        assert Path(repeated_save.path()).read_bytes() == payload
        assert page.get_by_role('dialog').count() == 0, 'Save again unexpectedly opened a dialog'

    atomic.check()
    save_button.click()
    dialog = page.get_by_role('dialog')
    dialog.get_by_label('File name').fill('explicit.tex')
    with page.expect_download() as alternate_extension:
        dialog.get_by_role('button', name='OK', exact=True).click()
    explicit_save = alternate_extension.value
    assert explicit_save.suggested_filename == 'explicit.tex', explicit_save.suggested_filename
    assert Path(explicit_save.path()).read_bytes() == payload

    fail_rename.check()
    save_button.click()
    dialog = page.get_by_role('dialog')
    dialog.get_by_label('File name').fill('rename-failure.txt')
    try:
        with page.expect_download(timeout=750):
            dialog.get_by_role('button', name='OK', exact=True).click()
    except PlaywrightTimeoutError:
        pass
    else:
        raise AssertionError('failed staged rename produced a download')
    assert page.locator('#status').get_attribute('data-state') == 'ready'
    assert not errors, errors
    print(json.dumps({'dom_mouse_and_keyboard': True, 'pascal_canvas_colors': True,
                      'dom_textarea_composition_unicode_and_ctrl_enter': True,
                      'checkbox_changes_lcl_paint': True, 'canvas_pointer_and_reset': True,
                      'anchored_resize': True, 'callback_cycles': 100,
                      'direct_download': True, 'default_extension': True,
                      'explicit_alternate_extension': True, 'atomic_rename_download': True,
                      'direct_repeat_download': True, 'atomic_repeat_download': True,
                      'failed_rename_no_download': True,
                      'job_before': before, 'job_after': after}))
    browser.close()
