# SPDX-License-Identifier: MIT
"""Behavioral browser checks against the ordinary Pascal demo's visible output."""
import argparse
import json
from playwright.sync_api import sync_playwright

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

    count = page.get_by_role('button', name='Count: 0', exact=True)
    count.click()
    page.wait_for_function('document.querySelector("#lcl-CounterButton").textContent === "Count: 1"')
    count = page.locator('#lcl-CounterButton')
    count.focus()
    page.keyboard.press('Enter')
    page.wait_for_function('document.querySelector("#lcl-CounterButton").textContent === "Count: 2"')
    assert count.evaluate('(el) => el === document.activeElement'), 'Repaint stole DOM focus'

    # The LFM/Pascal specifies light blue RGB(175,207,234), not a host-generated shape.
    assert pixel(158, 263) == [175, 207, 234, 255], pixel(158, 263)
    checkbox = page.get_by_role('checkbox', name='Filled shape')
    old = frame()
    checkbox.focus()
    page.keyboard.press('Space')
    painted_after(old)
    assert not checkbox.is_checked()
    assert pixel(158, 263) == [255, 255, 255, 255]
    old = frame()
    checkbox.click()
    painted_after(old)
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
    page.wait_for_function('document.querySelector("#lcl-CounterButton").textContent === "Count: 100"')
    after = page.evaluate('({live:jobHost.live(),slots:jobHost.slots()})')
    assert after['live'] == before['live'], (before, after)
    assert after['slots'] <= before['slots']+2, (before, after)

    # Exercise the platform resize boundary; the LFM's Anchors must grow the paintbox.
    old = frame()
    page.evaluate('lclDemo.lcl_resize(800,520)')
    painted_after(old)
    assert page.locator('#lcl').get_attribute('width') == '800'
    assert pixel(760, 490) == [255, 255, 255, 255], pixel(760, 490)
    assert not errors, errors
    print(json.dumps({'dom_mouse_and_keyboard': True, 'pascal_canvas_colors': True,
                      'checkbox_changes_lcl_paint': True, 'canvas_pointer_and_reset': True,
                      'anchored_resize': True, 'callback_cycles': 100,
                      'job_before': before, 'job_after': after}))
    browser.close()
