// Real RmlUi layout and mouse/keyboard regression coverage for the placed workshop.
void workshopLayoutTests(Rml::Context& context)
{
    using namespace management6a;
    StockpilePort port; StockpileScreen screen;
    Management6AController controller(port, screen);
    Management6ARmlBinding binding(context);
    check(binding.initialize(controller), "Workshop documents load");
    controller.addViewPort(binding); controller.beginWorld(WorldEpoch{81});
    WorkshopSnapshot workshop; workshop.id={91}; workshop.name="Crude workbench"; workshop.maxPriority=3;
    WorkshopProductRow plank; plank.id=CatalogId{"Plank"};
    plank.components.push_back({CatalogId{"RawWood"},1,true,{{CatalogId{"any"},150},{CatalogId{"AppleWood"},0}}});
    workshop.products.push_back(plank);
    workshop.products.push_back({CatalogId{"WoodChair"},plank.components});
    CraftQueueRow job; job.id={92}; job.craft=CatalogId{"Plank"}; job.count=4; job.mode=CraftRepeatMode::Maintain;
    workshop.queue.push_back(job);
    controller.showWorkshop(workshop,Revision{1},WorldPosition{1,2,3}); update(context);
    auto* doc=binding.workshopDocument();
    auto get=[&](const char* id){ return doc->GetElementById(id); };
    auto action=[&](const char* selector){ auto* e=doc->QuerySelector(selector); check(e!=nullptr,selector); click(context,e); };
    for(float scale : {1.f,1.25f,1.5f,2.f}) for(auto size : {Rml::Vector2i{560,400},Rml::Vector2i{660,950},Rml::Vector2i{900,640}})
    {
        const Rml::Vector2i pixels{int(size.x*scale),int(size.y*scale)};
        context.SetDensityIndependentPixelRatio(scale); context.SetDimensions(pixels);
        controller.setWorkshopPane(WorkshopPane::Craft); update(context);
        for(const char* id : {"workshop_view_craft","workshop_products","workshop_product_selection","workshop_order_count","workshop_queue_once","workshop_close"})
            check(inViewport(get(id),pixels),std::string("Workshop craft control fits: ")+id);
        check(get("workshop_product_selection")->GetChild(0)->GetOffsetWidth() > get("workshop_product_selection")->GetOffsetWidth() * .7f,
            "Narrow editor contents use the available width instead of collapsing to intrinsic text width");
        check(inViewport(get("workshop_material_0"),pixels),"Material selector remains within compact window");
        click(context,get("workshop_view_queue"));
        check(controller.state().workshop.pane==WorkshopPane::Queue,"Mouse opens queue pane");
        for(const char* id : {"workshop_queue","workshop_job_selection","workshop_job_count"})
            check(inViewport(get(id),pixels),std::string("Workshop queue control fits: ")+id);
        click(context,get("workshop_view_settings"));
        check(get("workshop_name")->IsVisible(true),"Settings accessible independently of crafting");
        get("workshop_stockpile_choice")->ScrollIntoView(); update(context);
        check(inViewport(get("workshop_stockpile_choice"),pixels) && inViewport(get("workshop_link_add"),pixels),"Linked stockpile controls fit after scrolling at every size and scale");
        get("workshop_priority")->ScrollIntoView(); update(context);
        check(inViewport(get("workshop_priority_up"),pixels) && inViewport(get("workshop_priority_down"),pixels),"Both priority steppers fit the window");
    }
    context.SetDensityIndependentPixelRatio(1.f); context.SetDimensions({900,640});
    controller.setWorkshopPane(WorkshopPane::Craft); update(context);
    click(context,get("workshop_order_count"));
    context.ProcessKeyDown(Rml::Input::KI_A,Rml::Input::KM_CTRL); context.ProcessKeyUp(Rml::Input::KI_A,Rml::Input::KM_CTRL);
    context.ProcessTextInput("23"); update(context);
    check(controller.state().workshop.orderCount==23,"Typing quantity updates draft without replacing input");
    auto* input=get("workshop_order_count");
    controller.showWorkshop(workshop,Revision{2},WorldPosition{1,2,3}); update(context);
    check(input==get("workshop_order_count") && context.GetFocusElement()==input,"Live snapshot preserves quantity element and keyboard focus");
    action("[data-order-mode=maintain]");
    check(controller.state().workshop.orderCount==23,"Changing order mode preserves typed count");
    click(context,get("workshop_material_0"));
    context.ProcessKeyDown(Rml::Input::KI_DOWN,0); context.ProcessKeyUp(Rml::Input::KI_DOWN,0);
    context.ProcessKeyDown(Rml::Input::KI_RETURN,0); context.ProcessKeyUp(Rml::Input::KI_RETURN,0); update(context);
    check(controller.state().workshop.orderMaterials.front().value=="AppleWood","Material dropdown keyboard selection reaches controller");
    auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(get("workshop_material_0"));
    for (int iteration=0; iteration<30; ++iteration)
    {
        click(context,select); update(context);
        ++workshop.products.front().components.front().materials.front().second;
        controller.showWorkshop(workshop,Revision{std::uint64_t(10+iteration)},WorldPosition{1,2,3}); update(context);
        check(select==get("workshop_material_0") && select->IsSelectBoxVisible(),"Live stock update preserves an open material dropdown");
        auto* option=select->GetOption(iteration%2);
        click(context,option);
        check(select==get("workshop_material_0"),"Mouse selection preserves the live dropdown through focus restoration");
    }
    click(context,get("workshop_queue_once"));
    check(!port.sent.empty() && port.sent.back().id.value=="workshop.queue_craft","Visible Add order dispatches craft command");
    auto payload=std::get<QueueCraftPayload>(port.sent.back().payload);
    check(payload.mode==CraftRepeatMode::Maintain && payload.count==23 && payload.materials.front().value=="AppleWood","Stock limit and unavailable material retained in authoritative command");
    check(get("workshop_queue_once")->GetInnerRML()=="Adding..." && get("workshop_queue_once")->HasAttribute("disabled"),"Add order immediately shows progress and blocks a duplicate click");
    controller.onWorkshopOrderResult(workshop.id,true); update(context);
    check(get("workshop_queue_once")->GetInnerRML()=="Add order" && get("workshop_order_feedback")->GetInnerRML()=="Order added to queue.","Accepted order receives visible confirmation");
    controller.setSearch("WoodChair");
    click(context,get("workshop_view_queue"));
    check(controller.state().workshop.visibleQueue.size()==1,"Recipe search never hides queued orders");
    check(binding.setFormValueForProbe("workshop_job_count","37"),"Set queue limit draft");
    controller.showWorkshop(workshop,Revision{40},WorldPosition{1,2,3}); update(context);
    action("[data-job-action=apply-count]");
    check(std::get<SetCraftJobPayload>(port.sent.back().payload).count==37,"Unrelated snapshot preserves edited queue limit until Apply");
    action("[data-job-action=toggle-suspended]");
    check(std::get<SetCraftJobPayload>(port.sent.back().payload).suspended,"Suspend order dispatches");
    action("[data-job-action=toggle-move-back]");
    check(std::get<SetCraftJobPayload>(port.sent.back().payload).moveBack,"Move back when done dispatches");
    for(auto pair : {std::pair{"front",MoveDirection::Front},std::pair{"up",MoveDirection::Up},std::pair{"down",MoveDirection::Down},std::pair{"back",MoveDirection::Back}})
    {
        action((std::string("[data-job-action=")+pair.first+"]").c_str());
        check(std::get<MoveCraftJobPayload>(port.sent.back().payload).direction==pair.second,"Queue movement dispatches");
    }
    action("[data-job-action=cancel]"); check(port.sent.back().id.value=="workshop.cancel_job","Cancel order dispatches");
    controller.setWorkshopPane(WorkshopPane::Settings); update(context);
    check(!get("workshop_refresh"),"Workshop refreshes automatically without a Refresh button");
    check(get("workshop_link_add")->HasAttribute("disabled"),"Link is disabled when no stockpiles exist");
    check(binding.setFormValueForProbe("workshop_name","Draft workshop"),"Set unapplied name draft");
    check(binding.setFormValueForProbe("workshop_priority","2"),"Set priority");
    check(std::get<SetWorkshopBasicsPayload>(port.sent.back().payload).priority==1,"Priority change applies immediately");
    workshop.stockpiles={{{101},"Stockpile1",false},{{102},"Stockpile2",false}};
    controller.showWorkshop(workshop,Revision{41},WorldPosition{1,2,3}); update(context);
    check(static_cast<Rml::ElementFormControl*>(get("workshop_name"))->GetValue()=="Draft workshop"
        && static_cast<Rml::ElementFormControl*>(get("workshop_priority"))->GetValue()=="2","Automatic snapshot preserves unapplied settings drafts");
    check(!get("workshop_link_add")->HasAttribute("disabled"),"New stockpiles become available on a live snapshot");
    check(binding.setFormValueForProbe("workshop_stockpile_choice","102"),"Select named stockpile");
    click(context,get("workshop_link_add"));
    auto link=std::get<SetWorkshopStockpileLinkPayload>(port.sent.back().payload);
    check(link.stockpile.value==102 && link.linked,"Link sends the selected stockpile ID");
    workshop.stockpiles[1].linked=true;
    controller.showWorkshop(workshop,Revision{42},WorldPosition{1,2,3}); update(context);
    action("[data-unlink='102']");
    link=std::get<SetWorkshopStockpileLinkPayload>(port.sent.back().payload);
    check(link.stockpile.value==102 && !link.linked,"Unlink affects only the chosen stockpile");
    click(context,get("workshop_priority_down"));
    check(std::get<SetWorkshopBasicsPayload>(port.sent.back().payload).priority==2,"Priority stepper applies its new value immediately");
    check(binding.setFormValueForProbe("workshop_priority","1"),"Set highest priority");
    click(context,get("workshop_apply_basics"));
    check(std::get<SetWorkshopBasicsPayload>(port.sent.back().payload).priority==0,"Priority uses one-based display and zero-based command");
    click(context,get("workshop_toggle_generated"));
    check(std::get<SetWorkshopBasicsPayload>(port.sent.back().payload).acceptGenerated,"Settings checkbox dispatches through the change event");
    workshop.id={93}; workshop.subtype="Fisher"; workshop.products.clear(); workshop.queue.clear();
    controller.showWorkshop(workshop,Revision{1},WorldPosition{1,2,3}); update(context);
    check(get("workshop_toggle_catch")->IsVisible(true),"Recipe-less fisher keeps specialty controls visible");
    get("workshop_toggle_catch")->ScrollIntoView(); update(context);
    click(context,get("workshop_toggle_catch")); check(port.sent.back().id.value=="workshop.set_fisher_options","Specialty action dispatches");
    controller.removeViewPort(binding); binding.shutdown(); update(context);
    std::cout << "Workshop: 12 size/scale layouts and real input/payload parity passed\n";
}
