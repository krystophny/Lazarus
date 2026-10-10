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
                      'checkbox_changes_lcl_paint': True, 'canvas_pointer_and_reset': True,
                      'anchored_resize': True, 'callback_cycles': 100,
                      'direct_download': True, 'default_extension': True,
                      'explicit_alternate_extension': True, 'atomic_rename_download': True,
                      'direct_repeat_download': True, 'atomic_repeat_download': True,
                      'failed_rename_no_download': True,
                      'job_before': before, 'job_after': after}))
    browser.close()
